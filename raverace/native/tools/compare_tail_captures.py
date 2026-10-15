#!/usr/bin/env python3
"""Compare every selected world/composed byte, with priority and mirror regions."""
import argparse,pathlib,numpy as np,json,sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'));from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('reference',type=pathlib.Path);p.add_argument('candidate',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();r=Reader(a.pack/'frames.rsc');rows=[]
files=sorted(a.reference.glob('tick-*.world.rgba'));assert files
for f in files:
 t=int(f.name.split('-')[1].split('.')[0]);scene=r.read(t);protected=np.zeros((480,640),bool);inset=np.zeros((480,640),bool)
 for first,count,page,mode,x,y,w,h in scene['groups']:
  if (x,y,w,h)!=(0,0,640,480):inset[max(0,y):min(480,y+h),max(0,x):min(640,x+w)]=True
  if mode&2:protected[max(0,y):min(480,y+h),max(0,x):min(640,x+w)]=True
 row=dict(tick=t,protected_scissor_pixels=int(protected.sum()),inset_scissor_pixels=int(inset.sum()))
 for kind in ['world','composed']:
  x=np.fromfile(a.reference/f'tick-{t}.{kind}.rgba',np.uint8).reshape(480,640,4);y=np.fromfile(a.candidate/f'tick-{t}.{kind}.rgba',np.uint8).reshape(480,640,4);delta=x!=y;row[kind+'_pixels']=int(np.any(delta,axis=2).sum());row[kind+'_protected_pixels']=int(np.any(delta,axis=2)[protected].sum());row[kind+'_inset_pixels']=int(np.any(delta,axis=2)[inset].sum())
  if kind=='world':row['priority_pixels']=int(delta[:,:,3].sum())
 rows.append(row)
result=dict(frames=len(rows),all_bytes_match=all(x['world_pixels']==x['composed_pixels']==0 for x in rows),samples=rows,scope='Protected/scissored regions include mirrors; whole-frame RGBA comparison also checks depth/priority consequences. This is raster equality against original native ordering, not new user visual signoff or all-frame Mali capture.')
a.out.write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['all_bytes_match']
