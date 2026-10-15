#!/usr/bin/env python3
"""Full lossless frame comparison; pixel statistics are not a fidelity pass oracle."""
import argparse,pathlib,json,statistics
import numpy as np
p=argparse.ArgumentParser();p.add_argument('artifacts',type=pathlib.Path);a=p.parse_args();rows=[]
for tick in range(1800,5640):
 x=np.fromfile(a.artifacts/'reference-sequence'/f'hud-{tick}.reference.rgba',np.uint8).reshape(480,640,4);y=np.fromfile(a.artifacts/'native-sequence'/f'tick-{tick}.rgba',np.uint8).reshape(480,640,4);e=np.abs(x.astype(np.int16)-y);rows.append(dict(tick=tick,mean_rgb_absolute_error=float(e[:,:,:3].mean()),changed_rgb_pixels=int(np.any(e[:,:,:3],axis=2).sum()),changed_alpha_pixels=int(np.count_nonzero(e[:,:,3])),maximum_channel_error=int(e.max())))
(a.artifacts/'sequence-pixels-final.json').write_text(json.dumps(rows,separators=(',',':'))+'\n');summary=dict(frames=len(rows),median_frame_mean_rgb_error=statistics.median(r['mean_rgb_absolute_error'] for r in rows),worst=sorted(rows,key=lambda r:r['mean_rgb_absolute_error'],reverse=True)[:10],frames_with_changed_final_alpha=sum(r['changed_alpha_pixels']!=0 for r in rows));(a.artifacts/'sequence-comparison-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
