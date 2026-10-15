#!/usr/bin/env python3
"""Export exact existing M3 mesh/material identities; no scene replay payload."""
import argparse,pathlib,json,hashlib,struct
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('audit',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
m=json.loads((a.pack/'meshes.json').read_text());materials=json.loads((a.pack/'materials.json').read_text());variants=json.loads((a.audit/'variants.json').read_text());rows=[]
for i,key in enumerate(sorted(materials)):
 v=materials[key];pal=bytes.fromhex(variants[key]['palette_rgb']);assert len(pal)==768 and hashlib.sha256(pal).hexdigest()==v['palette_sha256'];assert len(v['recipe'])==7;rows.append((tuple(v['recipe']),i,pal))
rows.sort(key=lambda x:(x[0],x[1]));m.sort(key=lambda x:(x['code'],x['packet']));b=struct.pack('<4I',0x344c5252,1,len(m),len(rows))+b''.join(struct.pack('<3i',x['code'],x['packet'],x['texel']) for x in m)+b''.join(struct.pack('<8i',*recipe,i)+pal for recipe,i,pal in rows);(a.out/'live.lookup').write_bytes(b)
report=dict(format='RR native live asset lookup',version=1,meshes=len(m),materials=len(rows),bytes=len(b),sha256=hashlib.sha256(b).hexdigest(),pack_manifest_sha256=hashlib.sha256((a.pack/'manifest.json').read_bytes()).hexdigest(),scope='Only immutable mesh/material identity and exact palette values; no frames, cameras, transforms, source command streams, fonts or HUD styles.')
(a.out/'lookup.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
