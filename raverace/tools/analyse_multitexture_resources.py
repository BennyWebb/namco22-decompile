#!/usr/bin/env python3
"""Attribute capture differences using P/T/V manifests; requires NumPy."""
from pathlib import Path
from collections import Counter
import numpy as np,json
import sys
root=Path(sys.argv[1])
def read(name):
 out={};p=None
 for line in (root/f'{name}.resources').read_text().splitlines():
  f=line.split()
  if f[0]=='P':p={'meta':f[1:],'v':[]};out[int(f[1])]=p
  elif f[0]=='T':p['texture']=f[1:]
  elif f[0]=='V':p['v'].append([float.fromhex(z) for z in f[1:]])
 return out
a,b=read('off'),read('batch')
def ppm(p):return np.frombuffer(p.read_bytes().split(b'\n',3)[3],np.uint8).reshape(480,640,3)
x0,x1=ppm(root/'off.ppm'),ppm(root/'batch.ppm');y,x=np.where(np.any(x0!=x1,2));points=np.c_[x+.5,y+.5]
changed=[];last={};covers={k:[] for k in range(len(points))}
for qi,p in a.items():
 q=b.get(qi);m=p['meta'].copy();m[1]='ID';n=q['meta'].copy();n[1]='ID'
 different=m!=n or p['texture']!=q['texture'] or p['v']!=q['v']
 if different:changed.append(qi)
 sx,sy,sw,sh=map(int,p['meta'][16:20]);clip=(points[:,0]>=sx)&(points[:,0]<sx+sw)&(480-points[:,1]>=sy)&(480-points[:,1]<sy+sh)
 for t in np.array(p['v']).reshape(-1,3,10):
  mat=np.vstack([t[1,:2]-t[0,:2],t[2,:2]-t[0,:2]]).T
  if abs(np.linalg.det(mat))<1e-12:continue
  uv=np.linalg.solve(mat,(points-t[0,:2]).T).T
  inside=clip&(uv[:,0]>=-1e-8)&(uv[:,1]>=-1e-8)&(uv.sum(1)<=1+1e-8)
  for k in np.where(inside)[0]:last[k]=qi;covers[k].append(qi)
print('pixels',len(points),'changed resources',changed,'last covering',Counter(last.values()))
print('pixel last covering resource differs',sum(last.get(k) in changed for k in range(len(points))))
for qi in sorted(set(last.values())):
 print('polygon',qi,'changed',qi in changed,'off texture',a[qi]['texture'],'batch texture',b[qi]['texture'])
summary={'pixels_differing':len(points),'polygons_with_resource_differences':changed,'last_covering':dict(Counter(last.values())),'pixels_last_resource_differs':sum(last.get(k) in changed for k in range(len(points)))}
(root/'resource-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
