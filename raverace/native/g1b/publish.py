#!/usr/bin/env python3
"""Publish lossless telemetry and hashes; never publish ROM assets/readback pixels."""
import pathlib,json,gzip,hashlib,shutil
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/g1b';dest=root/'raverace/benchmarks/native-g1b-20261016';manifest=[]
def digest(b):return hashlib.sha256(b).hexdigest()
def put(p,q):
 b=p.read_bytes();q.parent.mkdir(parents=True,exist_ok=True)
 if len(b)>16384:
  q=q.with_name(q.name+'.gz');q.write_bytes(gzip.compress(b,mtime=0));assert gzip.decompress(q.read_bytes())==b
 else:q.write_bytes(b)
 manifest.append(dict(source=str(p.relative_to(root)),stored=str(q.relative_to(dest)),source_bytes=len(b),source_sha256=digest(b),stored_sha256=digest(q.read_bytes())))
for name in ['colour-index','provenance-index','colour-final','provenance-final','colour-stream','smoke-validation','smoke-play','smoke-v2-validation','smoke-v2-play','smoke-stream']:
 p=base/name
 if p.exists():
  for f in sorted(p.iterdir()):
   if f.is_file() and f.suffix in ['.txt','.csv','.json','.jsonl','.log']:put(f,dest/'runs'/name/f.name)
for p in sorted((base/'device').iterdir()):
 if p.is_dir():
  for f in sorted(p.iterdir()):
   if f.is_file() and f.suffix in ['.txt','.csv','.json','.jsonl','.before','.after','.fixed','.start','.end','.governor','.max','.min','.max_freq','.min_freq','.sha256']:put(f,dest/'runs'/('device-'+p.name)/f.name)
for f in sorted(base.iterdir()):
 if f.is_file() and (f.suffix in ['.json','.log','.sha256'] or f.name in ['final-state.txt','transfer-verified.txt']):put(f,dest/'session'/f.name)
for name in ['reference-v2','b2-play/arm','b2-validation/arm','b2-validation/host']:
 f=base/name/'build.json'
 if f.exists():put(f,dest/'builds'/name.replace('/','-')/'build.json')
for f in sorted((base/'reference-v2').glob('*-build.json')):put(f,dest/'builds'/f.name)
readbacks=[]
for name in ['colour-final','provenance-final','colour-stream','device/colour-index','device/provenance-index']:
 for f in sorted((base/name).rglob('*')):
  if f.is_file() and f.suffix in ['.rgba','.u32','.f32','.i32']:readbacks.append(dict(path=str(f.relative_to(root)),bytes=f.stat().st_size,sha256=digest(f.read_bytes())))
(dest/'readback-manifest.json').write_text(json.dumps(readbacks,indent=2)+'\n');(dest/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
sources={str(f.relative_to(root)):dict(bytes=f.stat().st_size,sha256=digest(f.read_bytes())) for f in sorted((root/'raverace/native/g1b').iterdir()) if f.is_file()};(dest/'source-sha256.json').write_text(json.dumps(sources,indent=2)+'\n');print('Published',len(manifest),'telemetry files;',len(readbacks),'private readbacks hashed')
