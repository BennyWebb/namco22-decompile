#!/usr/bin/env python3
"""Publish reproducible telemetry, excluding packet bodies, pixels and raw strace."""
import argparse,gzip,hashlib,json,pathlib,shutil
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('destination',type=pathlib.Path);a=p.parse_args()
a.destination.mkdir(parents=True,exist_ok=True)
allowed={'.json','.jsonl','.csv','.txt','.uptime','.before','.after'}
manifest=[]
for source in sorted(a.root.rglob('*')):
 if not source.is_file() or source.name.startswith('syscalls.'):continue
 if source.suffix not in allowed:continue
 relative=source.relative_to(a.root);target=a.destination/'runs'/relative
 target.parent.mkdir(parents=True,exist_ok=True)
 data=source.read_bytes()
 if len(data)>65536:
  target=target.with_name(target.name+'.gz')
  with target.open('wb') as raw:
   with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as f:f.write(data)
 else:target.write_bytes(data)
 manifest.append(dict(file=str(target.relative_to(a.destination)),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),source_bytes=len(data),source_sha256=hashlib.sha256(data).hexdigest()))
(a.destination/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Published',len(manifest),'files; source bytes',sum(r['source_bytes'] for r in manifest))
