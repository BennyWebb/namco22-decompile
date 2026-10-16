#!/usr/bin/env python3
"""Lossless retained telemetry; raw perf and executable/ROM payloads stay ignored."""
import argparse,gzip,hashlib,json,pathlib
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('destination',type=pathlib.Path);a=p.parse_args();a.destination.mkdir(parents=True,exist_ok=True)
allowed={'.json','.jsonl','.csv','.txt','.uptime','.before','.after','.fixed','.sha256','.start','.end','.governor','.min','.max','.svg'}
manifest=[];private=[]
for source in sorted(a.root.rglob('*')):
 if not source.is_file():continue
 relative=source.relative_to(a.root)
 if source.name in ['perf.data','perf-script.txt','dmesg.txt']:
  private.append(dict(file=str(relative),bytes=source.stat().st_size,sha256=hashlib.sha256(source.read_bytes()).hexdigest(),retained_ignored_path=str(source)));continue
 if source.suffix not in allowed:continue
 data=source.read_bytes();target=a.destination/'runs'/relative;target.parent.mkdir(parents=True,exist_ok=True)
 if len(data)>65536 and source.suffix!='.svg':
  target=target.with_name(target.name+'.gz')
  with target.open('wb') as raw:
   with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as f:f.write(data)
 else:target.write_bytes(data)
 manifest.append(dict(file=str(target.relative_to(a.destination)),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),source_bytes=len(data),source_sha256=hashlib.sha256(data).hexdigest()))
(a.destination/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(a.destination/'private-profile-manifest.json').write_text(json.dumps(private,indent=2)+'\n');print('Published',len(manifest),'telemetry artifacts; private',len(private))
