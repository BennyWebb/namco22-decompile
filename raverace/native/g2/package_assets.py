#!/usr/bin/env python3
"""ROM-only coordinate plan and shader/material metadata; no pen/XYZ payloads."""
import pathlib,json,struct,shutil,hashlib
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g2';pack=pathlib.Path((b/'selected.txt').read_text().strip());out=b/'asset-kit';out.mkdir(exist_ok=True);template=out/'template';template.mkdir(exist_ok=True);m=json.loads((pack/'manifest.json').read_text());mapping=json.loads((pack/'mapping.json').read_text());meshes=json.loads((pack/'meshes.json').read_text());rects={}
for x in mapping:
 r=x['merged_recipe'];c=x['canonical_recipe'];page=x['page'];ox=x['rect'][0]-(c[2]-r[2]);oy=x['rect'][1]-(c[3]-r[3]);key=(page,ox,oy,r[4],r[5]);rects.setdefault(key,(*r,page,ox,oy))
dims=m['atlas']['page_dimensions'];height=(pack/'meshes.f32').stat().st_size//(1024*16);plan=struct.pack('<5I',0x31503247,len(rects),len(meshes),height,len(dims))+b''.join(struct.pack('<2I',*d) for d in dims)+b''.join(struct.pack('<9I',*r) for k,r in sorted(rects.items()))+b''.join(struct.pack('<2I',x['packet'],x['texel']) for x in meshes);(out/'layout.bin').write_bytes(plan)
files={}
for p in pack.iterdir():
 if not p.is_file() or p.suffix=='.json':continue
 if not p.name.startswith('page-') and p.name!='meshes.f32':shutil.copyfile(p,template/p.name)
 files[p.name]=dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
j=dict(format='g2-rom-cache-1',sources=m['sources'],files=files,models=2421,meshes=len(meshes),materials=m['materials'],plan_sha256=hashlib.sha256(plan).hexdigest());(out/'recipe.json').write_text(json.dumps(j,sort_keys=True)+'\n');print('Plan',len(rects),'rectangles',len(meshes),'meshes',len(plan),'bytes; no pixels/XYZ')
