#!/usr/bin/env python3
"""Complete replay audit before any asset expansion; partition boot/native needs."""
import argparse,pathlib,sys,json,hashlib,collections,numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'));from protocol import read,recipes
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest();enc=lambda v:(json.dumps(v,sort_keys=True,separators=(',',':'))+'\n').encode();oldm=json.loads((a.pack/'meshes.json').read_text());oldmat=json.loads((a.pack/'materials.json').read_text());oldids={(x['code'],x['packet']) for x in oldm};oldkeys=set(oldmat);meshes={};variants={};missing_meshes={};unsupported=collections.Counter();rows=[];hashes={};maxima=dict(commands=0,direct=0)
for tick in range(1,6007):
 f=read(a.capture/f'tick-{tick}.bin',1,6006);q=f['quads'];rr=recipes(f);pal=np.frombuffer(f['palette'],np.uint8).reshape(3,128,256).transpose(1,2,0).copy();groups={g:sha(pal[g].tobytes()) for g in {r[1] for r in rr}};phase='native' if tick>=1800 else 'legacy_boot';missing=set();absent=set();dynamic=solid=0
 for r in rr:
  key=sha(json.dumps([r,groups[r[1]]],separators=(',',':')).encode())
  if key not in variants:variants[key]=dict(recipe=r,palette_rgb=pal[r[1]].tobytes().hex(),first_tick=tick,last_tick=tick,native=False,legacy_boot=False,uses=0)
  v=variants[key];v[phase]=True;v['last_tick']=tick;v['uses']+=1
  if key not in oldkeys:missing.add(key)
 for row in q:
  identity=(int(row[0]),int(row[1]));dynamic+=int(row[0]==5);solid+=int(row[7]!=0)
  if row[0]==5:continue
  data=row[36:60].reshape(4,6)[:,:5].copy();key=f'{identity[0]}:{identity[1]}'
  if key not in meshes:meshes[key]=dict(code=identity[0],packet=identity[1],first_tick=tick,last_tick=tick,native=False,legacy_boot=False,local_sha256=sha(data.tobytes()),uses=0)
  v=meshes[key];assert v['local_sha256']==sha(data.tobytes()),('mutable static mesh',tick,identity);v[phase]=True;v['last_tick']=tick;v['uses']+=1
  if identity not in oldids:absent.add(key)
 unsupported[phase+'_point_ram_quads']+=dynamic;unsupported[phase+'_solid_quads']+=solid;maxima['commands']=max(maxima['commands'],len(q)+len(f['direct']));maxima['direct']=max(maxima['direct'],len(f['direct']));hashes[str(tick)]=f['sha256'];rows.append(dict(tick=tick,phase=phase,quads=len(q),direct=len(f['direct']),missing_meshes=len(absent),missing_material_variants=len(missing),point_ram=dynamic,solid=solid))
 if tick%1000==0:print('audited',tick,flush=True)
missing_meshes={k:v for k,v in meshes.items() if (v['code'],v['packet']) not in oldids};newvars={k:v for k,v in variants.items() if k not in oldkeys}
report=dict(ticks=6006,first=1,last=6006,commands=sum(r['quads']+r['direct'] for r in rows),mesh_identities=len(meshes),material_variants=len(variants),missing_meshes=len(missing_meshes),missing_material_variants=len(newvars),missing_native_meshes=sum(v['native'] for v in missing_meshes.values()),missing_native_material_variants=sum(v['native'] for v in newvars.values()),missing_legacy_only_meshes=sum(not v['native'] for v in missing_meshes.values()),missing_legacy_only_material_variants=sum(not v['native'] for v in newvars.values()),unsupported=dict(unsupported),maxima=maxima,original_pack_manifest_sha256=sha((a.pack/'manifest.json').read_bytes()),scope='Complete replay; legacy boot1..1799 separated explicitly from required live-native1800..6006. No missing occurrence ignored.')
for name,v in [('coverage.json',report),('meshes.json',meshes),('missing-meshes.json',missing_meshes),('variants.json',variants),('missing-variants.json',newvars),('ticks.json',rows),('packet-sha256.json',hashes)]: (a.out/name).write_bytes(enc(v))
print(json.dumps(report,indent=2))
