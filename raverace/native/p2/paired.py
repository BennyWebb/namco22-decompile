#!/usr/bin/env python3
"""Tick-matched service differences, including slower samples (positive=saved)."""
import argparse,pathlib,csv,json,statistics,math
p=argparse.ArgumentParser();p.add_argument('before',type=pathlib.Path);p.add_argument('after',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
def read(b):return {int(x['frame']):x for x in csv.DictReader((b/'thread.csv').open())}
x,y=read(a.before),read(a.after);assert sorted(x)==sorted(y)==list(range(1,6007))
metrics=[('prep_ms','prep_wall',1000),('compiler_ms','native_compile_ms',1),('walk_ms','native_walk_ms',1),('GL_CPU_ms','cpu',1000)]
rows=[]
for tick in range(1800,6007):rows.append(dict(tick=tick,**{k:(float(x[tick][col])-float(y[tick][col]))*scale for k,col,scale in metrics}))
a.output.parent.mkdir(exist_ok=True,parents=True)
with a.output.with_suffix('.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def dist(v):
 s=sorted(v);n=len(s);return dict(n=n,mean=statistics.mean(s),median=statistics.median(s),p05=s[math.ceil(.05*n)-1],p95=s[math.ceil(.95*n)-1],minimum=s[0],maximum=s[-1])
out=dict(before=a.before.name,after=a.after.name,meaning='before-minus-after per matched tick; positive=saved; repeatability/causality not established by one pair',windows={window:{k:dist(r[k] for r in rows if lo<=r['tick']<=hi) for k,_,_ in metrics} for window,lo,hi in [('native',1800,6006),('dense',3550,3590)]})
a.output.with_suffix('.json').write_text(json.dumps(out,indent=2)+'\n');print(out['before'],out['after'],out['windows']['dense']['prep_ms'])
