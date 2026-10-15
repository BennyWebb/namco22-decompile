#!/usr/bin/env python3
"""Complete-window world priority and composed-image accounting; metrics not an oracle."""
import argparse,pathlib,json,numpy as np
p=argparse.ArgumentParser();p.add_argument('reference',type=pathlib.Path);p.add_argument('candidate',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();changed=[];priority=0;total_world=total_composed=0
for t in range(1800,5640):
 row=dict(tick=t)
 for kind in ['world','composed']:
  x=np.fromfile(a.reference/f'tick-{t}.{kind}.rgba',np.uint8).reshape(480,640,4);y=np.fromfile(a.candidate/f'tick-{t}.{kind}.rgba',np.uint8).reshape(480,640,4);delta=np.abs(x.astype(np.int16)-y.astype(np.int16));n=int(np.any(delta,axis=2).sum());row[kind+'_pixels']=n;row[kind+'_mean_rgb']=float(delta[:,:,:3].mean());row[kind+'_max']=int(delta.max())
  if kind=='world':priority+=int(np.count_nonzero(delta[:,:,3]));total_world+=n
  else:total_composed+=n
 if row['world_pixels'] or row['composed_pixels']:changed.append(row)
a.out.write_text(json.dumps(dict(frames=3840,changed_world_priority_pixels=priority,total_changed_world_pixels=total_world,total_changed_composed_pixels=total_composed,different_frames=changed),indent=2)+'\n');print(dict(frames=3840,world_priority_changes=priority,world_pixels=total_world,composed_pixels=total_composed,different_frames=len(changed)))
