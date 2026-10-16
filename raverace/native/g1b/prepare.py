#!/usr/bin/env python3
"""Reuse unmodified ROM-only G1 cache; pad only debug metadata and validation maps."""
import pathlib,json,struct,os,math
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1b';b.mkdir(exist_ok=True);old=root/'raverace/native/generated/g1';pack=pathlib.Path((old/'selected.txt').read_text().strip());out=b/'pack';out.mkdir(exist_ok=True)
for p in pack.iterdir():
 if p.is_file() and p.name!='provenance-mapping.i32':
  q=out/p.name
  if not q.exists():q.symlink_to(p)
n=json.loads((pack/'manifest.json').read_text())['materials'];data=(pack/'provenance-mapping.i32').read_bytes();data+=bytes(1024*math.ceil((n+512)/1024)*32-len(data));(out/'provenance-mapping.i32').write_bytes(data)
lookup=(root/'raverace/native/generated/milestone4a-coverage/lookup/live.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',lookup);rows={}
for i in range(nt):
 r=struct.unpack_from('<8i',lookup,16+nm*12+i*800);rows.setdefault(tuple(r[:7]),r[7])
data=(old/'reference-identity.bin').read_bytes()+struct.pack('<I',len(rows))+b''.join(struct.pack('<8i',*r,mid) for r,mid in sorted(rows.items()));(b/'reference-identity.bin').write_bytes(data);(b/'selected.txt').write_text(str(out)+'\n');print(out)
