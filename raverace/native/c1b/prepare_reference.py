#!/usr/bin/env python3
import pathlib,json,struct
root=pathlib.Path(__file__).resolve().parents[3];out=root/'raverace/native/generated/c1b/reference';source=root/'raverace/native/generated/milestone4a-coverage/race-live-v2';m=json.loads((source/'materials.json').read_bytes());mapping={tuple(x['recipe']):x for x in json.loads((out/'mapping.json').read_bytes())};dims=json.loads((out/'sizing.json').read_bytes())['page_dimensions'];blob=b'';groups=b''
for v in sorted(m.values(),key=lambda x:x['id']):
 b,g,c,u,wv,w,h=v['recipe'];x=mapping[(b,c,u,wv,w,h)];page=x['page'];px,py,_,_=x['rect'];pw,ph=dims[page];uv=[1/pw,1/ph,(px+2.5-u)/pw,(py+2.5-wv)/ph];blob+=struct.pack('<4fI12x',*uv,page);groups+=struct.pack('<I',g)
(out/'materials.bin').write_bytes(blob);(out/'groups.bin').write_bytes(groups);(out/'atlas.meta').write_text(str(len(dims))+'\n'+''.join(f'{w} {h}\n' for w,h in dims))
shaders=root/'raverace/native/generated/stage1b/hardware-shaders'
for name in ['race_v3.vert','race_v3_near.vert']:
 s=(shaders/name).read_text().replace('out highp vec2 texcoord;','flat out highp int palette_group;\nout highp vec2 texcoord;').replace('material=int(abs(object_state.w))-1;', 'material=int(abs(object_state.w))/128-1;palette_group=int(abs(object_state.w))%128;');(out/name).write_text(s)
s='''#version 300 es
precision highp float;
precision highp int;
uniform highp usampler2D image;
uniform highp sampler2D palette;
flat in highp int palette_group;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
out vec4 colour;
void main(){ivec2 size=textureSize(image,0);ivec2 xy=clamp(ivec2(floor(texcoord*vec2(size))),ivec2(0),size-1);uint pen=texelFetch(image,xy,0).r;
vec4 t=texelFetch(palette,ivec2(int(pen),palette_group),0);
CUTOUT
colour=vec4(t.rgb*(shade_z.x/shade_z.y),priority);}
'''
(out/'index.frag').write_text(s.replace('CUTOUT',''))
(out/'index_cutout.frag').write_text(s.replace('CUTOUT','if(t.a==0.0)discard;'))
print('Reference indexed material metadata and shaders prepared; all reference alpha proven 255.')
