"""Standard-library summaries; API times are CPU wall durations."""
import csv,json,statistics,sys
from pathlib import Path
from collections import defaultdict

def percentile(v,p):
 v=sorted(v);return v[min(len(v)-1,int((len(v)-1)*p))]

def probe(directory):
 groups=defaultdict(list)
 for p in Path(directory).glob('samples-*.csv'):
  for r in csv.DictReader(p.open()):groups[(r['case'],int(r['w']),int(r['h']))].append(r)
 out=[]
 for (case,w,h),rs in groups.items():
  o=dict(case=case,w=w,h=h,n=len(rs),busy_before=sum(r['busy_before']=='1' for r in rs),busy_after=sum(r['busy_after']=='1' for r in rs))
  for k in ['gen_us','bind_us','alloc_us','upload_us','delete_us','complete_us']:
   v=[float(r[k]) for r in rs];o[k]=dict(median=statistics.median(v),p95=percentile(v,.95),p99=percentile(v,.99),maximum=max(v))
  out.append(o)
 return out

def trace(directory):
 out={}
 for p in Path(directory).glob('*.calls.csv'):
  rows=list(csv.DictReader(p.open()));result={}
  for label,subset in [('burst',[r for r in rows if 1963<=int(r['tick'])<=1967]),('lap',[r for r in rows if 1800<=int(r['tick'])<=5639])]:
   g=defaultdict(list)
   for r in subset:g[r['op']].append(r)
   result[label]={op:dict(n=sum(int(r['updates']) if r['id']=='0' and r['w']=='0' else 1 for r in rs),wall_ms=sum(float(r['wall_us']) for r in rs)/1000,bytes=sum(int(r['bytes']) for r in rs),p95_us=percentile([float(r['wall_us']) for r in rs],.95)) for op,rs in g.items()}
  out[p.stem]=result
 return out
if __name__=='__main__':print(json.dumps(probe(sys.argv[2]) if sys.argv[1]=='probe' else trace(sys.argv[2]),indent=2))
