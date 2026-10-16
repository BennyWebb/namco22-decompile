#!/usr/bin/env python3
"""Lossless raw telemetry; no ROM pack, binary or pixel data in published evidence."""
import pathlib,json,gzip,hashlib
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1c';out=root/'raverace/benchmarks/native-g1c-20261016';out.mkdir(exist_ok=True);manifest=[]
def sha(b):return hashlib.sha256(b).hexdigest()
def put(p,q):
 data=p.read_bytes();q.parent.mkdir(parents=True,exist_ok=True)
 if len(data)>16384:q=q.with_name(q.name+'.gz');q.write_bytes(gzip.compress(data,mtime=0));assert gzip.decompress(q.read_bytes())==data
 else:q.write_bytes(data)
 manifest.append(dict(source=str(p.relative_to(root)),stored=str(q.relative_to(out)),source_bytes=len(data),source_sha256=sha(data),stored_sha256=sha(q.read_bytes())))
for name in ['attribution','attribution-clean','fixed-attract','colour-index','provenance-index','device/smoke','device/reference','device/attract']:
 p=b/name
 if not p.exists():continue
 for f in sorted(p.iterdir()):
  if f.is_file() and f.suffix not in ['.bin','.u32','.rgba']:put(f,out/'runs'/name.replace('/','-')/f.name)
for f in sorted((b/'device-session').iterdir()):
 if f.is_file():put(f,out/'session/device-session'/f.name)
for f in sorted(b.iterdir()):
 if f.is_file() and f.suffix in ['.json','.log','.sha256','.txt']:put(f,out/'session'/f.name)
for name in ['b2-play/host','b2-play/arm','b2-validation/host','observe-build']:
 p=b/name/'build.json'
 if p.exists():put(p,out/'builds'/name.replace('/','-')/'build.json')
readbacks=[]
for name in ['colour-index','provenance-index']:
 for f in sorted((b/name/'images').iterdir()):
  if f.is_file():readbacks.append(dict(source=str(f.relative_to(root)),bytes=f.stat().st_size,sha256=sha(f.read_bytes())))
(out/'readback-manifest.json').write_text(json.dumps(readbacks,indent=2)+'\n');(out/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'source-sha256.json').write_text(json.dumps({str(p.relative_to(root)):sha(p.read_bytes()) for p in sorted(pathlib.Path(__file__).parent.iterdir()) if p.is_file()},indent=2)+'\n');print('Published',len(manifest),'telemetry files;',len(readbacks),'ignored readbacks hashed')
