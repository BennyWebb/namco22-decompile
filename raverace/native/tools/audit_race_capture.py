#!/usr/bin/env python3
"""Account every race tick, source mesh, palette recipe and original projection."""
import argparse,sys,pathlib,json,hashlib,collections
import numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'))
from protocol import read,recipes
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
variants={};meshes={};models=set();palettes=set();groups=collections.defaultdict(set);rows=[];total=direct=checked=dynamic=solid=fog=0;hashes={};maxq=0
for tick in range(1800,5640):
 f=read(a.capture/f'tick-{tick}.bin');assert f['tick']==tick;hashes[str(tick)]=f['sha256'];q=f['quads'];n=len(q);r=recipes(f);total+=n;direct+=len(f['direct']);maxq=max(maxq,n+len(f['direct']));models.update(map(int,q[:,0]));dynamic+=int((q[:,0]==5).sum());solid+=int((q[:,7]!=0).sum())
 pal=np.frombuffer(f['palette'],np.uint8).reshape(3,128,256).transpose(1,2,0).copy();palettes.add(hashlib.sha256(f['palette']).hexdigest());gh={g:hashlib.sha256(pal[g].tobytes()).hexdigest() for g in {x[1] for x in r}}
 ids=[]
 for recipe in r:
  key=hashlib.sha256(json.dumps([recipe,gh[recipe[1]]],separators=(',',':')).encode()).hexdigest();ids.append(key)
  if key not in variants:variants[key]={'recipe':recipe,'palette_rgb':pal[recipe[1]].tobytes().hex(),'first_tick':tick,'last_tick':tick}
  variants[key]['last_tick']=tick;groups[recipe[1]].add(gh[recipe[1]])
 if n:
  local=q[:,36:60].reshape(n,4,6);oracle=q[:,60:].reshape(n,4,7);matrix=q[:,12:21].reshape(n,3,3);trans=q[:,21:24];xyz=(np.einsum('nvr,nrc->nvc',local[:,:,:3].astype(np.int64),matrix.astype(np.int64))>>15)+trans[:,None,:]
  valid=oracle[:,:,6]!=0
  assert np.array_equal(xyz[:,:,2][valid],oracle[:,:,2][valid]);assert np.array_equal(local[:,:,3:][valid],oracle[:,:,3:6][valid]);checked+=int(valid.sum())
  # Match original signed truncation and screen saturation without float divides.
  z=np.where(xyz[:,:,2]==0,1,xyz[:,:,2]);shift=q[:,25,None];mant=q[:,24,None];cent=q[:,26:28]
  for c,sgn in [(0,1),(1,-1)]:
   v=(xyz[:,:,c]*mant*16)>>shift;div=(np.abs(v)//np.abs(z))*np.where((v<0)!=(z<0),-1,1);screen=np.clip(cent[:,None,c]*16+sgn*div,-2147483648,2147483647)
   assert np.array_equal(screen[valid],oracle[:,:,c][valid]),(tick,c)
  for i,row in enumerate(q):
   identity=(int(row[0]),int(row[1]));b=local[i,:,:5].tobytes()
   if identity in meshes:assert meshes[identity]==b,'static source geometry changed'
   else:meshes[identity]=b
 rows.append(dict(tick=tick,quads=n,direct=len(f['direct']),material_variants=len(set(ids)),footprint_bytes=sum((variants[k]['recipe'][5]+4)*(variants[k]['recipe'][6]+4)*4 for k in set(ids)),packet_bytes=(a.capture/f'tick-{tick}.bin').stat().st_size))
report=dict(ticks=len(rows),first_tick=1800,last_tick=5639,static_quads=total,direct_quads=direct,max_primitives=maxq,models=sorted(models),static_source_packets=len(meshes),valid_source_corners_checked=checked,projection_differences=0,runtime_point_ram_quads=dynamic,palette_solid_quads=solid,palette_snapshots=len(palettes),palette_variants_by_group={str(k):len(v) for k,v in sorted(groups.items())},material_variants=len(variants),variant_gutter_bytes=sum((v['recipe'][5]+4)*(v['recipe'][6]+4)*4 for v in variants.values()),max_frame_recipe_gutter_bytes=max(r['footprint_bytes'] for r in rows),capture_bytes=sum(r['packet_bytes'] for r in rows))
for name,value in [('audit.json',report),('ticks.json',rows),('variants.json',variants),('packet-sha256.json',hashes)]: (a.out/name).write_text(json.dumps(value,sort_keys=True,separators=(',',':'))+'\n')
print(json.dumps(report,indent=2))
