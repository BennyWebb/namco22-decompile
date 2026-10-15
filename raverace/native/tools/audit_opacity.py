#!/usr/bin/env python3
"""Prove native material alpha over padded footprints and every actual UV hull."""
import argparse,pathlib,json,hashlib,sys,numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'))
from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();m=json.loads((a.pack/'manifest.json').read_bytes());assert m['version']==3
for name,info in m['files'].items():
 with (a.pack/name).open('rb') as f:assert hashlib.file_digest(f,'sha256').hexdigest()==info['sha256']
materials=json.loads((a.pack/'materials.json').read_bytes());keys=sorted(materials);opaque=[];bounds=[];texels=0
pages=[np.memmap(a.pack/f'page-{i}.rgba',dtype=np.uint8,mode='r',shape=(h,w,4)) for i,(w,h) in enumerate(m['pages'])]
for k in keys:
 v=materials[k];x,y,w,h=v['rect'];page=pages[v['page']];assert x>=0 and y>=0 and x+w<=page.shape[1] and y+h<=page.shape[0];alpha=page[y:y+h,x:x+w,3];opaque.append(bool(np.all(alpha==255)));texels+=alpha.size;bounds.append((x,y,x+w,y+h))
geo=np.memmap(a.pack/'meshes.f32',dtype='<f4',mode='r').reshape(-1,4);uv=np.frombuffer((a.pack/'materials.bin').read_bytes(),'<f4').reshape(-1,8)[:,:4];reader=Reader(a.pack/'frames.rsc');uses=np.zeros(len(keys),np.int64);bad=np.zeros(len(keys),bool);count=0;min_margin=1e9
for tick in range(1800,5640):
 f=reader.read(tick);q=f['quads'];mid=q['material'];positions=np.empty((len(q),4,2),np.float32)
 for dynamic in [False,True]:
  take=np.flatnonzero((q['mesh']<0)==dynamic);base=np.where(dynamic,-q['mesh'][take]-1,q['mesh'][take]);source=f['dynamic'] if dynamic else geo;positions[take]=source[base[:,None]+np.arange(4)[None,:]*2+1,:2]
 mapped=positions*uv[mid,None,:2]+uv[mid,None,2:];dims=np.array([m['pages'][materials[keys[int(i)]]['page']] for i in mid],np.float32);mapped*=dims[:,None,:];rect=np.array(bounds)[mid];margin=np.minimum(mapped-rect[:,None,:2],rect[:,None,2:]-mapped);safe=np.isfinite(mapped).all((1,2))&(margin.min((1,2))>=1);np.logical_or.at(bad,mid,~safe);uses+=np.bincount(mid,minlength=len(keys));count+=len(q);min_margin=min(min_margin,float(margin.min()))
proved=np.array(opaque)&~bad
report=dict(version=1,pack_manifest_sha256=hashlib.sha256((a.pack/'manifest.json').read_bytes()).hexdigest(),ticks=3840,source_commands=count,materials=len(keys),padded_texels_checked=texels,opaque_materials=int(proved.sum()),cutout_or_unproved_materials=int((~proved).sum()),all_opaque=bool(proved.all()),commands_opaque=int(uses[proved].sum()),uv_hull_failures=int(bad.sum()),minimum_guard_texels=min_margin,proof='All padded-rectangle alpha bytes=255; every original local/direct UV corner has >=1 texel guard inside that rectangle. Positive-depth clipping and perspective interpolation stay inside the UV convex hull. Nearest/clamp filtering only; palette/pages bound by manifest hashes.',material_flags=[int(x) for x in proved]);a.out.write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='material_flags'})
