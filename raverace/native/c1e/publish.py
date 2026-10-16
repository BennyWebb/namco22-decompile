#!/usr/bin/env python3
"""Keep every telemetry sample losslessly; never publish binary ROM/readback data."""
import pathlib,gzip,hashlib,json
root=pathlib.Path(__file__).resolve().parents[3];src=root/'raverace/native/generated/c1e';dst=root/'raverace/benchmarks/native-c1e-20261016';manifest=[];readbacks=[]
allowed={'.json','.jsonl','.csv','.txt','.uptime','.before','.after','.fixed','.sha256','.start','.end','.governor','.min','.max'}
for run in sorted(src.iterdir()):
 if not run.is_dir() or not (run.name.startswith('device-') or run.name.startswith('smoke-')):continue
 for p in sorted(run.rglob('*')):
  if not p.is_file():continue
  rel=pathlib.Path(run.name)/p.relative_to(run);data=p.read_bytes();h=hashlib.sha256(data).hexdigest()
  if 'images' in p.relative_to(run).parts:
   readbacks.append(dict(file=str(p.relative_to(root)),bytes=len(data),sha256=h));continue
  if p.suffix not in allowed:continue
  out=dst/'runs'/rel;out.parent.mkdir(parents=True,exist_ok=True)
  if len(data)>65536:out=out.with_name(out.name+'.gz');out.write_bytes(gzip.compress(data,mtime=0))
  else:out.write_bytes(data)
  manifest.append(dict(file=str(out.relative_to(dst)),bytes=len(data),source_sha256=h,stored_sha256=hashlib.sha256(out.read_bytes()).hexdigest()))
(dst/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(dst/'readback-manifest.json').write_text(json.dumps(readbacks,indent=2)+'\n');print('Retained',len(manifest),'telemetry artifacts;',len(readbacks),'private readbacks')
