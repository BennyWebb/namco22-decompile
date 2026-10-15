#!/usr/bin/env python3
"""Native local-mesh/instance snapshot format; never imports captured triangles."""
import argparse,hashlib,json,math,os,pathlib,shutil,struct
import numpy as np
p=argparse.ArgumentParser();p.add_argument('models',type=pathlib.Path);p.add_argument('instances',type=pathlib.Path);p.add_argument('materials',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--reference',type=pathlib.Path);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[3];assert not a.out.exists() and not a.out.resolve().is_relative_to(repo);a.out.mkdir(parents=True)
def enc(x):return (json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
def sha(x):return hashlib.sha256(x).hexdigest()
manifest=json.loads((a.materials/'manifest.json').read_bytes());assert sha((a.models/'manifest.json').read_bytes())==manifest['native_asset_manifest_sha256']
model_manifest=json.loads((a.models/'manifest.json').read_bytes())
assert manifest['format']=='rr-native-material-pages' and manifest['version']==1
for n,d in manifest['files'].items():
 b=(a.materials/n).read_bytes();assert len(b)==d['bytes'] and sha(b)==d['sha256']
mat=json.loads((a.materials/'materials.json').read_bytes());keys=sorted(mat['materials']);material_ids={k:i for i,k in enumerate(keys)}
source=[json.loads(s) for s in (a.instances/'quads.jsonl').read_text().splitlines()]
assert sha((a.instances/'quads.jsonl').read_bytes())==manifest['source_snapshot_sha256']
models={};vertex_data=bytearray();model_table=[];instance_data=[];instance_ids={};quads=[];links=mat['source_links'];model_offsets={}
for code in sorted({r['code'] for r in source}):
 m=json.loads((a.models/f'model-{code}.json').read_bytes());offset=len(vertex_data)//24;model_offsets[code]=offset;models[code]=m
 raw=(a.models/f'model-{code}.vertices').read_bytes();assert sha(raw)==model_manifest['files'][f'model-{code}.vertices']['sha256'];vertex_data.extend(raw);model_table.append(dict(code=code,first_vertex=offset,vertices=len(raw)//24,source_sha256=sha(raw)))
def add_instance(code,m,t,cam):
 key=(code,tuple(m),tuple(t),tuple(cam))
 if key not in instance_ids:
  instance_ids[key]=len(instance_data);instance_data.append(struct.pack('<12i4f',*m,*t,*cam))
 return instance_ids[key]
def protected(r):
 v=r['vertices'];l,rr,t,b=r['clip'];valid=[p for p in v if p[6]]
 if len(valid)!=4:return False
 xy=np.array([p[:2] for p in v],float)/16
 if xy[:,0].max()<l or xy[:,0].min()>rr or xy[:,1].max()<t or xy[:,1].min()>b:return False
 for ids in [(0,1,2),(0,2,3)]:
  x,y,z=xy[list(ids)];area=(y[0]-x[0])*(z[1]-x[1])-(z[0]-x[0])*(y[1]-x[1])
  if area==0:return True
 return False
ndirect=len(json.loads((a.materials/'direct.json').read_bytes())) if (a.materials/'direct.json').exists() else 0
for ordinal,r in enumerate(source):
 assert r['objectflags']==0,'palette-solid instance mode requires explicit native material implementation'
 link=links[ordinal];assert link['code']==r['code'] and link['source_data']==r['source_data'];info=mat['materials'][link['material']]
 cam=[r['center'][0]/320-1,1-r['center'][1]/240,math.ldexp(r['zoom'][0],-r['zoom'][1])/320,math.ldexp(r['zoom'][0],-r['zoom'][1])/240]
 inst=add_instance(r['code'],r['m'],r['t'],cam);l,rr,t,b=r['clip'];box=[max(0,l),479-min(479,b),min(639,rr)-max(0,l)+1,min(479,b)-max(0,t)+1]
 quads.append(dict(first=model_offsets[r['code']]+link['first_vertex'],instance=inst,page=info['page'],flags=2 if protected(r) else 0,box=box,info=info,material=material_ids[link['material']],alpha=int((r['cmode']&7)==1),code=r['code'],packet=link['packet_address'],zsort=r['zsort'],order=ndirect+ordinal))
for r,link in zip(json.loads((a.materials/'direct.json').read_bytes()) if ndirect else [],links[len(source):]):
 assert r['index']==link['direct_index'];raw=r['raw'];info=mat['materials'][link['material']];first=len(vertex_data)//24
 for i in range(4):
  words=raw[4+6*i:10+6*i];sx=struct.unpack('<h',struct.pack('<H',words[2]))[0]+320;sy=struct.unpack('<h',struct.pack('<H',words[3]))[0]+240
  ooz=math.ldexp(words[5],-max(0,46-(words[4]&63))) if words[5] else 65536
  z=min(2000000000,max(1,int(1/ooz if ooz>0 else 1)))
  vertex_data.extend(struct.pack('<6i',sx,sy,z,words[0]&4095,words[1]&4095,words[4]>>8))
 inst=add_instance(-1,[32768,0,0,0,32768,0,0,0,32768],[0,0,0],[0,0,1/320,1/240])
 quads.append(dict(first=first,instance=inst,page=info['page'],flags=1,box=[0,0,640,480],info=info,material=material_ids[link['material']],alpha=int((r['recipe'][2]&7)==1),code=0xffffffff,packet=r['index'],zsort=r['zsort'],order=r['index']))
quads.sort(key=lambda r:(-r['zsort'],-r['order']));qbytes=bytearray()
for i,r in enumerate(quads):
 info=r['info'];bank,group,cmode,u,v,w,h=info['recipe'];g=info['gutter'];ww,hh=info['rect'][2:];native=info['native_uv_affine'];individual=[1/ww,1/hh,(g+.5-u)/ww,(g+.5-v)/hh];depth=.9-1.8*(i+1)/(len(quads)+1)
 qbytes.extend(struct.pack('<4I4i10f3I',r['first'],r['instance'],r['page'],r['flags'],*r['box'],*native,*individual,depth,r['alpha'],r['code'],r['packet'],r['material']))
assert len(qbytes)==84*len(quads)
(a.out/'models.bin').write_bytes(vertex_data);(a.out/'instances.bin').write_bytes(b''.join(instance_data));(a.out/'quads.bin').write_bytes(qbytes)
(a.out/'source-models.json').write_bytes(enc(model_table))
textures=bytearray()
for key in keys:
 info=mat['materials'][key];textures.extend(struct.pack('<5I',info['page'],*info['rect']))
(a.out/'textures.bin').write_bytes(textures)
for i,(w,h) in enumerate(mat['pages']):
 try:os.link(a.materials/f'page-{i}.rgba',a.out/f'page-{i}.rgba')
 except OSError:shutil.copyfile(a.materials/f'page-{i}.rgba',a.out/f'page-{i}.rgba')
background=list(map(float,(a.reference/'scene.meta').read_text().split()[:3])) if a.reference else [0,0,0]
meta=f'RRNATIVE1 {len(mat["pages"])} {len(vertex_data)//24} {len(instance_data)} {len(quads)} {len(keys)} '+ ' '.join(map(str,background))+'\n'+''.join(f'{w} {h}\n' for w,h in mat['pages'])
(a.out/'scene.meta').write_text(meta)
files={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(a.out.iterdir())}
scene_manifest=dict(format='rr-native-local-scene',version=1,source_material_manifest_sha256=sha((a.materials/'manifest.json').read_bytes()),native_asset_manifest_sha256=manifest['native_asset_manifest_sha256'],source_geometry='original local-space vertices + captured Q15 instances; no projected triangle import',files=files,source_quads=len(source),direct_quads=ndirect,protected_source_quads=sum(bool(r['flags']&2) for r in quads),estimated_grouped_draws=len(mat['pages'])*2+sum(bool(r['flags']&2) for r in quads),primitive_count=len(quads))
(a.out/'manifest.json').write_bytes(enc(scene_manifest));print(json.dumps({k:v for k,v in scene_manifest.items() if k!='files'},indent=2))
