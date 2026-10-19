#!/usr/bin/env python3
"""Deterministic whole-ROM atlas, mesh and material compiler. No replay inputs."""
from pathlib import Path
import argparse,hashlib,json,math,shutil,struct,time
import numpy as np
from rom import ROM,validate_model
from atlas import build
p=argparse.ArgumentParser();p.add_argument('roms',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
out=a.output.resolve();rom=ROM(a.roms.resolve());t=time.monotonic()
source_files=sorted(Path(__file__).resolve().parent.glob('*.py'))+sorted((Path(__file__).resolve().parents[1]/'shaders').iterdir())
compiler_sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()}
if out.exists():
 recipe=json.loads((out/'asset-kit/recipe.json').read_text())
 assert recipe['sources']==rom.sources and recipe['compiler_sources']==compiler_sources,'asset compiler/cache version changed; use a fresh output directory'
 for name,v in recipe['files'].items():assert hashlib.sha256((out/name).read_bytes()).hexdigest()==v['sha256'],name
 print('verified ROM compiler cache',out);raise SystemExit(0)
out.mkdir(parents=True,exist_ok=False)
encode=lambda x:(json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
models=[];recipes=set();geometry=[];meshes=[];inventory=[];nexttexel=0
for code in range(69,2490):
 if rom.word(code)<0:continue
 m=rom.model(code);validate_model(m);models.append(code)
 for q in m['quads']:
  verts=np.asarray(m['vertices'][q['first_vertex']:q['first_vertex']+4],dtype=np.int32);u=verts[:,3];v=verts[:,4];b,g,c=q['material'];r=(b,g,c,int(u.min()),int(v.min()),int(u.max()-u.min()+1),int(v.max()-v.min()+1));recipes.add(r)
  data=np.zeros((8,4),'<f4');data[::2,:3]=verts[:,:3];data[1::2,:2]=verts[:,3:5];geometry.append(data);packet=q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4);meshes.append({'code':code,'packet':packet,'texel':nexttexel});nexttexel+=8
  inventory.append({'code':code,'packet':packet,'recipe':r,'lighting_packets':q['lighting_packet_count']})
assert len(models)==2421 and len(meshes)==97116
sizing,mapping,pages=build(rom,{(r[0],*r[2:]) for r in recipes},out,'whole-ROM')
mp={tuple(m['recipe']):m for m in mapping};dims=sizing['page_dimensions'];height=math.ceil(nexttexel/1024);geo=np.zeros((height*1024,4),'<f4');geo[:nexttexel]=np.concatenate(geometry);(out/'meshes.f32').write_bytes(geo.tobytes());(out/'meshes.json').write_bytes(encode(meshes))
materials=[];matbytes=bytearray();groups=bytearray();lookup=[];meta=bytearray(1024*math.ceil((len(recipes)+512)/1024)*32)
for i,r in enumerate(sorted(recipes)):
 b,g,c,u,v,w,h=r;m=mp[(b,c,u,v,w,h)];page=m['page'];x,y,_,_=m['rect'];pw,ph=dims[page];uv=[1/pw,1/ph,(x+2.5-u)/pw,(y+2.5-v)/ph];materials.append({'id':i,'recipe':r,'page':page,'rect':m['rect'],'uv':uv});matbytes+=struct.pack('<4fI12x',*uv,page);groups+=struct.pack('<I',g);lookup.append(struct.pack('<8i',*r,i));struct.pack_into('<8i',meta,i*32,b,u,v,x+2,y+2,w,h,g)
assert len(materials)<=65535
(out/'materials.json').write_bytes(encode(materials));(out/'materials.bin').write_bytes(matbytes);(out/'groups.bin').write_bytes(groups);(out/'provenance-mapping.i32').write_bytes(meta)
(out/'gles3.lookup').write_bytes(struct.pack('<4I',0x4b4c314e,1,len(meshes),len(materials))+b''.join(struct.pack('<3i',m['code'],m['packet'],m['texel']) for m in sorted(meshes,key=lambda m:(m['code'],m['packet'])))+b''.join(lookup))
(out/'atlas.meta').write_text(str(len(dims))+'\n'+''.join(f'{w} {h}\n' for w,h in dims));(out/'gles3.meta').write_text(f'GLES3 {len(dims)} 1024 {height} {len(materials)}\n')

# Coordinate-only plan is generated locally, not distributed through Git.
rects={}
for x in mapping:
 r=x['merged_recipe'];c=x['canonical_recipe'];page=x['page'];ox=x['rect'][0]-(c[2]-r[2]);oy=x['rect'][1]-(c[3]-r[3]);key=(page,ox,oy,r[4],r[5]);rects.setdefault(key,(*r,page,ox,oy))
plan=struct.pack('<5I',0x31503247,len(rects),len(meshes),height,len(dims))+b''.join(struct.pack('<2I',*d) for d in dims)+b''.join(struct.pack('<9I',*r) for k,r in sorted(rects.items()))+b''.join(struct.pack('<2I',m['packet'],m['texel']) for m in meshes)
kit=out/'asset-kit';kit.mkdir();(kit/'layout.bin').write_bytes(plan);template=kit/'template';template.mkdir()
files={}
for name in ['atlas.meta','gles3.meta','gles3.lookup','materials.bin','groups.bin','provenance-mapping.i32','meshes.f32']+[f'page-{i}.r8ui' for i in range(len(dims))]:
 q=out/name;files[name]=dict(bytes=q.stat().st_size,sha256=hashlib.sha256(q.read_bytes()).hexdigest())
 if name!='meshes.f32' and not name.startswith('page-'):shutil.copyfile(q,template/name)
for q in (Path(__file__).resolve().parents[1]/'shaders').iterdir():
 if q.suffix not in ('.frag','.vert'):continue
 shutil.copyfile(q,template/q.name);shutil.copyfile(q,out/q.name);files[q.name]=dict(bytes=q.stat().st_size,sha256=hashlib.sha256(q.read_bytes()).hexdigest())
(kit/'recipe.json').write_bytes(encode(dict(format='gles3-rom-cache-1',compiler_sources=compiler_sources,sources=rom.sources,files=files,plan_sha256=hashlib.sha256(plan).hexdigest())))
(out/'materials.json').write_bytes(encode(materials));print('compiled',len(meshes),'meshes',len(materials),'materials in',time.monotonic()-t,'seconds')
