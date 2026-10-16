#!/usr/bin/env python3
"""Validation identities ONLY. Never called by assets.py or used to build ROM packs."""
import pathlib,json,struct
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1';p=pathlib.Path((b/'selected.txt').read_text().strip());m=json.loads((p/'manifest.json').read_text());old=root/'raverace/native/generated/milestone4a-coverage/race-live-v2';oldmesh={(x['code'],x['packet']):x['texel'] for x in json.loads((old/'meshes.json').read_text())};lookup=(root/'raverace/native/generated/milestone4a-coverage/lookup/live.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',lookup);oldmat={}
for i in range(nt):
 row=struct.unpack_from('<8i',lookup,16+nm*12+i*800);oldmat.setdefault(tuple(row[:7]),row[7])
a=np.full(m['geometry_bytes']//16,-1,'<i4')
for x in json.loads((p/'meshes.json').read_text()):
 if (x['code'],x['packet']) in oldmesh:a[x['texel']]=oldmesh[x['code'],x['packet']]
mat=np.asarray([oldmat.get(tuple(x['recipe']),-1) for x in json.loads((p/'materials.json').read_text())],'<i4');data=struct.pack('<2I',len(a),len(mat))+a.tobytes()+mat.tobytes();out=b/'reference-identity.bin'
if out.exists():assert out.read_bytes()==data
else:out.write_bytes(data)
print('Validation identities:',sum(mat>=0),'materials;',sum(a>=0),'meshes')
