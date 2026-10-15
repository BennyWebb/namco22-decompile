#!/usr/bin/env python3
"""Diagnostic 32x24 pixel-centre sampling; NOT a GPU counter or image oracle."""
from pathlib import Path
import sys,json,numpy as np,csv
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scene'));from compact_protocol import Reader
p=Path('raverace/native/generated/milestone3/race-v3');out=Path('raverace/benchmarks/native-tail-hud-20261015');r=Reader(p/'frames.rsc');m=np.fromfile(p/'meshes.f32',dtype='<f4').reshape(-1,4)
rows=list(csv.DictReader((out/'frame-features.csv').open()));slow=[x for x in rows if float(x['total_ms'])>16.693];ticks=set(int(float(x['tick'])) for x in slow)
# Deterministic spread of all other pictures, preserving difficult/faster variety.
ticks.update(range(1800,5640,56));ticks.update(json.loads((out/'slow-analysis.json').read_text())['capture_ticks'])
y,x=np.mgrid[0:480:20,0:640:20];points=np.stack([x.ravel()+.5,y.ravel()+.5],axis=1);result=[]
def triangle(a,b,c):
 edges=[]
 for u,v in [(a,b),(b,c),(c,a)]:edges.append((points[:,0]-u[0])*(v[1]-u[1])-(points[:,1]-u[1])*(v[0]-u[0]))
 e=np.array(edges);return np.all(e>=0,axis=0)|np.all(e<=0,axis=0) if abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))>1e-8 else np.zeros(len(points),bool)
for tick in sorted(ticks):
 f=r.read(tick);q=f['quads'];ids=f['visible'];masks={};depths=q['depth'];front=ids.copy()
 for first,count,page,mode,bx,by,bw,bh in f['groups']:
  first,count,mode=int(first),int(count),int(mode)
  if mode==0:front[first:first+count]=ids[first:first+count][::-1]
  scissor=(points[:,0]>=bx)&(points[:,0]<bx+bw)&(points[:,1]>=by)&(points[:,1]<by+bh)
  for index in ids[first:first+count]:
   quad=q[index];base=int(quad['mesh']);local=(m if base>=0 else f['dynamic'])[(base if base>=0 else -base-1)+np.arange(4)*2,:3];inst=f['instances'][quad['instance'],:16].reshape(4,4);pos=np.floor(local@inst[:3,:3].T/32768)+inst[:3,3]
   if mode&1:
    clipped=[]
    for i in range(4):
     a,b=pos[i],pos[(i+1)%4];ina,inb=a[2]>=1,b[2]>=1
     if ina:clipped.append(a)
     if ina!=inb:
      t=(1-a[2])/(b[2]-a[2]);v=a+(b-a)*t;v[2]=1;clipped.append(v)
    pos=np.array(clipped)
   if len(pos)<3:masks[int(index)]=np.zeros(len(points),bool);continue
   xy=(pos[:,:2]/pos[:,2,None]*inst[3,2:]+inst[3,:2]+1)*[320,240];mask=np.zeros(len(points),bool)
   for j in range(1,len(xy)-1):mask|=triangle(xy[0],xy[j],xy[j+1])
   masks[int(index)]=mask&scissor
 def simulate(order):
  depth=np.full(len(points),1.);passed=0
  for i in order:
   hit=masks[int(i)]&(depths[i]<=depth);passed+=int(hit.sum());depth[hit]=depths[i]
  return passed
 fragments=sum(int(v.sum()) for v in masks.values());result.append(dict(tick=tick,sampled_coverage=fragments/len(points),original_depth_passes=simulate(ids),front_depth_passes=simulate(front),samples=len(points)))
report=dict(method='32x24 original pixel-centre samples, source projection/near clipping, group scissors, triangle coverage and constant synthetic depth. No shader/texture cost model; NOT hardware counters or exact full-resolution overdraw. Unioned triangles per quad; raster top-left edge rules approximated.',frames=len(result),rows=result)
(out/'coarse-overdraw.json').write_text(json.dumps(report,indent=2)+'\n');print('sampled',len(result),'pictures')
