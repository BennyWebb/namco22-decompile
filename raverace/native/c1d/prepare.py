#!/usr/bin/env python3
"""Private shader + exact per-material inverse atlas mappings; no alias controls."""
import pathlib,json,struct,shutil
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1d';src=root/'raverace/native/generated/milestone4a-coverage/race-live-v2';mats=json.loads((src/'materials.json').read_bytes());compact=root/'raverace/native/generated/c1b/reference';mapping={tuple(x['recipe']):x for x in json.loads((compact/'mapping.json').read_bytes())}
for mode in ['rgba','index']:
 out=base/mode;out.mkdir(exist_ok=True)
 if mode=='index':
  for p in compact.iterdir():
   if p.is_file() and p.suffix in ['.bin','.meta','.r8ui']:shutil.copyfile(p,out/p.name)
 # Regular colour shaders preserve the C1c tested normalized/lowp path exactly.
 shaderroot=root/'raverace/native/generated/stage1b/hardware-shaders' if mode=='rgba' else root/'raverace/native/generated/c1c/compact'
 for name in ['race_v3.vert','race_v3_near.vert']:
  s=(shaderroot/name).read_text();(out/name).write_text(s)
  s=s.replace('out highp vec2 texcoord;','flat out highp int source_material;\nout highp vec2 texcoord;')
  token='vec4 m0=texelFetch';assert token in s;s=s.replace(token,'source_material=material;\n '+token)
  (out/('provenance_'+name)).write_text(s)
 for name in ['race.frag','index_texture_lowp.frag','index_fetch_lowp.frag']:
  p=(root/'raverace/native/generated/stage1b/hardware-shaders/race.frag') if name=='race.frag' else root/'raverace/native/generated/c1c/compact'/name
  if p.exists():shutil.copyfile(p,out/name)
 metadata=bytearray(1024*((len(mats)+1023)//1024)*2*16)
 for m in mats.values():
  b,g,c,u,v,w,h=m['recipe'];rec=m if mode=='rgba' else mapping[(b,c,u,v,w,h)];x,y,_,_=rec['rect'];off=m['id']*32;struct.pack_into('<8i',metadata,off,b,u,v,x+2,y+2,w,h,g)
 (out/'provenance-mapping.i32').write_bytes(metadata)
 grid='uniform lowp sampler2D coordinate_image;' if mode=='rgba' else 'uniform highp usampler2D coordinate_image;'
 decode='uvec4 bytes=uvec4(floor(texture(coordinate_image,texcoord)*255.0+0.5));' if mode=='rgba' else 'uvec4 bytes=texture(coordinate_image,texcoord);'
 sample='vec4 t=texture(image,texcoord);' if mode=='rgba' else 'uint pen=texture(image,texcoord).r;vec4 t=texelFetch(palette,ivec2(int(pen),palette_group),0);'
 header='uniform lowp sampler2D image;' if mode=='rgba' else 'uniform highp usampler2D image;uniform lowp sampler2D palette;flat in highp int palette_group;'
 frag='''#version 300 es
precision highp float;
precision highp int;
HEADER
GRID
uniform highp isampler2D source_mapping;
flat in highp int source_material;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
layout(location=0) out highp uvec4 provenance;
layout(location=1) out vec4 colour_proof;
layout(location=2) out vec4 sampled_texel;
void main(){DECODE
ivec2 xy=ivec2(bytes.x|(bytes.y<<8),bytes.z|(bytes.w<<8));
int at=source_material*2;ivec4 a=texelFetch(source_mapping,ivec2(at%1024,at/1024),0),b=texelFetch(source_mapping,ivec2((at+1)%1024,(at+1)/1024),0);
provenance=uvec4(a.x,(a.y+xy.x-a.w)&4095,(a.z+xy.y-b.x)&4095,source_material+1);
SAMPLE
colour_proof=vec4(t.rgb*(shade_z.x/shade_z.y),priority);sampled_texel=t;
}
'''.replace('HEADER',header).replace('GRID',grid).replace('DECODE',decode).replace('SAMPLE',sample)
 (out/'provenance.frag').write_text(frag)
print('Prepared RGBA/index provenance shaders, exact inverse mappings, lowp colour path.')
