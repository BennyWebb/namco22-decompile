#!/usr/bin/env python3
"""Reproduce the fixed previous-candidate 69-tail correlation investigation."""
from pathlib import Path
import sys,re,numpy as np,json,csv
sys.path.insert(0,'raverace/native/scene');from compact_protocol import Reader
p=Path('raverace/native/generated/milestone3/race-v3');out=Path('raverace/benchmarks/native-tail-hud-20261015');r=Reader(p/'frames.rsc')
def lines(path,prefix):return [{k:float(v) for k,v in re.findall(r'(\w+)=(-?[\d.]+)',l)} for l in Path(path).read_text().splitlines() if l.startswith(prefix)]
log='raverace/benchmarks/native-depth-lut-20261015/candidate-1.txt';frames=lines(log,'FRAME ');hud=lines(log,'HUDPROFILE ')[-3840:];meshes=np.fromfile(p/'meshes.f32',dtype='<f4').reshape(-1,4)
rows=[]
for f,h in zip(frames,hud):
 tick=int(f['tick']);s=r.read(tick);q=s['quads'][s['visible']];inst=s['instances'][q['instance'],:16].reshape(-1,4,4);local=np.empty((len(q),4,3));static=q['mesh']>=0
 for mask,mesh in [(static,meshes),(~static,s['dynamic'])]:
  base=q['mesh'][mask];base=np.where(base<0,-base-1,base);local[mask]=mesh[base[:,None]+np.arange(4)[None,:]*2,:3]
 pos=np.floor(np.einsum('nkj,nij->nki',local,inst[:,:3,:3])/32768)+inst[:,None,:3,3];z=np.maximum(pos[:,:,2],1);xy=pos[:,:,:2]/z[:,:,None]*inst[:,None,3,2:]+inst[:,None,3,:2];xy=(xy+1)*np.array([320,240]);lo=np.maximum(xy.min(axis=1),0);hi=np.minimum(xy.max(axis=1),[640,480]);area=np.prod(np.maximum(hi-lo,0),axis=1)
 groups=s['groups'];near=sum(int(g[1]) for g in groups if g[3]&1);protected=sum(int(g[1]) for g in groups if g[3]&2)
 prev=r.read(tick-1) if tick>1800 else None
 row=dict(f,pen_wall_ms=h['pen_wall_ms'],near_quads=near,protected_quads=protected,unique_materials=len(np.unique(q['material'])),material_transitions=int(np.count_nonzero(np.diff(q['material']))),page_transitions=int(np.count_nonzero(np.diff(groups[:,2]))),bbox_coverage_layers=float(area.sum()/307200),near_vertices=int(near*12),glyph_vertices=h['glyph_vertices'],shadow_enabled=h['shadow_enabled'],style=s['header'][9],style_changed=int(prev is None or prev['header'][9]!=s['header'][9]),tilemap_changed=int(prev is None or prev['text']!=s['text']));rows.append(row)
keys=list(rows[0]);(out/'frame-features.csv').write_text('')
with (out/'frame-features.csv').open('w') as f:w=csv.DictWriter(f,keys);w.writeheader();w.writerows(rows)
slow=[x for x in rows if x['total_ms']>16.693];assert len(slow)==69
features=['draws','primitives','visible','vertices','near_quads','protected_quads','unique_materials','material_transitions','page_transitions','bbox_coverage_layers','glyph_vertices','style_changed','tilemap_changed','wait_ms','prepare_ms','upload_ms','world_submit_ms','hud_submit_ms','finish_ms','cpu_ms','pen_wall_ms']
# Pen spans are CPU submissions in this log, not independent GPU execution.
for x,h in zip(rows,hud):x['pen_wall_ms']=h['pen_wall_ms']
y=np.array([x['total_ms'] for x in rows]);corr={}
for k in features:
 v=np.array([x[k] for x in rows]);corr[k]=dict(pearson=float(np.corrcoef(v,y)[0,1]) if v.std() else None,all_median=float(np.median(v)),slow_median=float(np.median([x[k] for x in slow])))
ticks={1800,1965,2500,2643,3000,3546,3600,4200,4800,5400,5639}
for k in ['total_ms','draws','near_quads','bbox_coverage_layers','protected_quads','glyph_vertices']:ticks.update(int(x['tick']) for x in sorted(rows,key=lambda x:x[k],reverse=True)[:2])
report=dict(frames=3840,slow_frames=69,commands=sum(x['primitives'] for x in rows),slow_ticks=[int(x['tick']) for x in slow],capture_ticks=sorted(ticks),correlations=corr,worst=sorted(rows,key=lambda x:x['total_ms'],reverse=True)[:15],limitations='bbox_coverage_layers is summed clipped screen bounding-box area, a density proxy, NOT measured fragment overdraw. Timing correlation is not causal attribution. Unfenced submit/finish spans overlap GPU work.')
(out/'slow-analysis.json').write_text(json.dumps(report,indent=2)+'\n');print('capture ticks',','.join(map(str,sorted(ticks))));print(json.dumps(corr,indent=2))
