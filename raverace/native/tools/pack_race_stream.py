#!/usr/bin/env python3
"""Bounded sequential native scene container; payloads identical to validated frames."""
import argparse,pathlib,struct,json,hashlib
p=argparse.ArgumentParser();p.add_argument('race',type=pathlib.Path);a=p.parse_args();root=a.race
index=json.loads((root/'frames.json').read_bytes());offset=16+(len(index)+1)*8;offsets=[offset]
for r in index:offset+=r['bytes'];offsets.append(offset)
with (root/'frames.rsc').open('xb') as out:
 out.write(struct.pack('<4I',0x33525352,2,len(index),0));out.write(struct.pack('<'+'Q'*len(offsets),*offsets))
 for r in index:
  b=(root/'frames'/f'{r["tick"]}.bin').read_bytes();assert hashlib.sha256(b).hexdigest()==r['sha256'];out.write(b)
assert (root/'frames.rsc').stat().st_size==offset
m=json.loads((root/'manifest.json').read_bytes())
with (root/'frames.rsc').open('rb') as f:h=hashlib.file_digest(f,'sha256').hexdigest()
m['files']['frames.rsc']=dict(bytes=offset,sha256=h);(root/'manifest.json').write_text(json.dumps(m,sort_keys=True,separators=(',',':'))+'\n');print(offset,h)
