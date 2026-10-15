#!/usr/bin/env python3
"""Publish P1 hashes/telemetry, excluding ROM-derived packets and pixel captures."""
import pathlib,shutil,gzip,hashlib,json,csv
root=pathlib.Path(__file__).resolve().parents[3];src=root/'raverace/native/generated/p1/hardware';dst=root/'raverace/benchmarks/native-p1-20261015'
small=['exit.txt','states.jsonl','method.txt','clocks.before','clocks.after','production.before','production.after','preflight.txt','es-restored.txt','es-start.txt','es-stop.txt','flag-validation.json','image-comparison.json','command-sha256.csv','p1-summary.json']
compressed=['thread.csv','ticks.csv','run.txt','memory.txt','stalls.json']
for p in sorted(src.iterdir()):
 if not p.is_dir() or not (p/'thread.csv').exists():continue
 d=dst/p.name;d.mkdir(exist_ok=True)
 for n in small:
  if (p/n).exists():shutil.copy2(p/n,d/n)
 for n in compressed:
  if (p/n).exists():
   with (p/n).open('rb') as f,gzip.GzipFile(filename=str(d/(n+'.gz')),mode='wb',mtime=0) as g:shutil.copyfileobj(f,g)
for n in ['matrix-status.txt','matrix-history.txt','matrix-console.log','integrity-before.txt','integrity-after-reduced-matrix.txt','run-validation.json','observer-operation-analysis.json','all-run-metrics.csv','measurement-counts.json']:
 if (src/n).exists():shutil.copy2(src/n,dst/n)
buildlogs=dst/'build-logs';buildlogs.mkdir(exist_ok=True)
for p in pathlib.Path('/tmp').glob('rr-p1-*build*.txt'):shutil.copy2(p,buildlogs/p.name)
archives={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (src.parent).glob('*.tgz')};(dst/'archive-sha256.json').write_text(json.dumps(archives,indent=2)+'\n')
stalls=[]
for p in sorted(src.glob('reference-*/stalls.json')):
 for s in json.loads(p.read_text()):
  stalls.append(dict(run=p.parent.name,frame=s['frame'],native=s['native'],gap_ms=s['gap_ms'],simulation_ms=s['simulation_ms'],fifo_blocked_ms=s['fifo_blocked_ms'],main_D_observed=s['main_D_state_observed'],worker_D_observed=s['worker_D_state_observed'],interpretation=s['interpretation']))
with (dst/'all-main-progress-stalls.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(stalls[0]));w.writeheader();w.writerows(stalls)
print('published',len(list(dst.rglob('*'))),'files/directories')
