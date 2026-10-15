#!/usr/bin/env python3
"""Verify native scene ABI, checksums, source associations and per-instance inputs."""
import argparse,hashlib,json,pathlib,struct
import numpy as np
p=argparse.ArgumentParser();p.add_argument('scene',type=pathlib.Path);p.add_argument('models',type=pathlib.Path);p.add_argument('materials',type=pathlib.Path);a=p.parse_args()
def sha(b):return hashlib.sha256(b).hexdigest()
m=json.loads((a.scene/'manifest.json').read_bytes());assert m['format']=='rr-native-local-scene' and m['version']==1
assert m['native_asset_manifest_sha256']==sha((a.models/'manifest.json').read_bytes())
assert m['source_material_manifest_sha256']==sha((a.materials/'manifest.json').read_bytes())
for name,info in m['files'].items():
 assert pathlib.Path(name).name==name
 b=(a.scene/name).read_bytes();assert len(b)==info['bytes'] and sha(b)==info['sha256'],name
head=(a.scene/'scene.meta').read_text().split();assert head[0]=='RRNATIVE1';npages,nv,ni,nq,nm=map(int,head[1:6])
source=np.frombuffer((a.scene/'models.bin').read_bytes(),'<i4').reshape(-1,6);assert len(source)==nv
instances=list(struct.iter_unpack('<12i4f',(a.scene/'instances.bin').read_bytes()));assert len(instances)==ni
quads=list(struct.iter_unpack('<4I4i10f3I',(a.scene/'quads.bin').read_bytes()));assert len(quads)==nq
textures=list(struct.iter_unpack('<5I',(a.scene/'textures.bin').read_bytes()));assert len(textures)==nm
material=json.loads((a.materials/'materials.json').read_bytes());assert npages==len(material['pages'])
models={};tables=json.loads((a.scene/'source-models.json').read_bytes());offsets={r['code']:r['first_vertex'] for r in tables};checked=direct=protected=0;pages=set();view_boxes=set()
for q in quads:
 first,instance,page,flags=q[:4];box=q[4:8];uv=np.array(q[8:12]);suv=np.array(q[12:16]);depth,alpha=q[16:18];code,packet,material_id=q[18:]
 assert first+4<=nv and instance<ni and page<npages and material_id<nm and flags&~3==0
 assert -1<depth<1 and alpha in (0,1) and np.isfinite(uv).all() and np.isfinite(suv).all()
 assert box[0]>=0 and box[1]>=0 and box[2]>0 and box[3]>0 and box[0]+box[2]<=640 and box[1]+box[3]<=480
 inst=instances[instance];assert np.isfinite(inst[12:]).all()
 info=material['materials'][sorted(material['materials'])[material_id]];assert page==info['page']
 assert np.array_equal(uv,np.array(info['native_uv_affine'],np.float32))
 if flags&1:assert code==0xffffffff;direct+=1
 else:
  if code not in models:
   model=json.loads((a.models/f'model-{code}.json').read_bytes());models[code]=(model,{x['packet_address']:x for x in model['quads']})
  model,by_packet=models[code];qsource=by_packet[packet];local=np.array(model['vertices'][qsource['first_vertex']:qsource['first_vertex']+4],np.int32)
  assert first==offsets[code]+qsource['first_vertex']
  assert np.array_equal(source[first:first+4],local)
  checked+=1
 protected+=bool(flags&2);pages.add(page);view_boxes.add(tuple(box))
assert checked==m['source_quads'] and direct==m['direct_quads'] and protected==m['protected_source_quads']
# Preserve every source corner; no captured projected vertices are needed here.
print(json.dumps(dict(source_quads=checked,direct_quads=direct,source_corners=4*checked,instances=ni,models=len(models),pages=len(pages),scissor_regions=len(view_boxes),protected_quads=protected,checksum_source_uv_or_index_errors=0),sort_keys=True))
