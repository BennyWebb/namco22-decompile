#!/usr/bin/env python3
"""Validate controlled read experiment; keep performance/count/sample phases separate."""
import csv,hashlib,json,pathlib,re,statistics,sys
root=pathlib.Path(sys.argv[1]);rows=[];states={};boards={};videos={};counts={}
for path in sorted(root.glob('*-*-*-*.log')):
 workload,phase,trial,mode=path.stem.split('-')
 if workload not in ('city','race') or phase not in ('performance','counts','profile') or mode not in ('baseline','fast'):continue
 lines=path.read_text().splitlines()
 def fields(prefix):return dict(re.findall(r'(\w+)=([^ ]+)',next(l for l in reversed(lines) if l.startswith(prefix))))
 frame=1700 if workload=='city' else 3600
 raw=path.with_suffix('.json').read_bytes();state=json.loads(raw)
 assert path.with_suffix('.exit').read_text().strip()=='0'
 assert state['frames']==frame and state['traps']==state['unmapped']==state['romwrites']==0
 assert not state['dsp_fault'] and not state['sound_fault']
 assert states.setdefault(workload,raw)==raw
 rate=fields('[SIM-RATE] ');clock=fields('[GEOMETRY-RATE] ');memory=fields('[MEM-READS] ')
 assert int(rate['steps'])==frame-1399
 assert int(rate['rendered'])+int(rate['skipped'])==int(rate['steps'])
 assert memory['fast']==('on' if mode=='fast' else 'off')
 assert memory['counters']==('on' if phase=='counts' else 'off')
 assert boards.setdefault(workload,fields('[BOARD-STATE] '))==fields('[BOARD-STATE] ')
 video=fields('[GEOMETRY] ');assert video['effective']=='on' and video['test_present_every']=='0'
 digest={k:video[k] for k in ('ticks','walks','direct_consumed','digest')}
 assert videos.setdefault(workload,digest)==digest
 row=dict(workload=workload,phase=phase,trial=trial,mode=mode,**rate,resyncs=clock['resyncs'],discarded_lag_ms=clock['discarded_lag_ms'],board_fnv1a32=fields('[BOARD-STATE] ')['fnv1a32'],state_sha256=hashlib.sha256(raw).hexdigest())
 row['RSS_peak_mib']=float(fields('[MEMORY] ')['RSS_peak_kib'])/1024
 if phase=='profile':
  stats=fields('[SIM] ');assert int(stats['ticks'])==frame-1399 and stats['overflow']=='0'
  assert float(stats['reconciliation_max_us'])<0.01
  for l in lines:
   if l.startswith('[SIM-STAGE] '):
    s=dict(re.findall(r'(\w+)=([^ ]+)',l))
    for k in ('avg_ms','median_ms','p95_ms','max_ms','percent'):row[s['name']+'_'+k]=s[k]
  assert fields('[SIM-SAMPLES] ')['overflow']=='0'
 else:assert not any(l.startswith('[SIM-STAGE]') for l in lines)
 if phase=='counts':
  counts[(workload,mode)]=memory
  for k,v in memory.items():
   if k not in ('fast','counters'):row['reads_'+k]=v
  total=sum(int(memory[k]) for k in ('byte','word','long','other_width'))
  assert total==sum(int(memory[k]) for k in ('ROM','WRAM','POLY','IO_bucket','other_bucket'))
  assert total==int(memory['hits'])+int(memory['fallbacks'])
 rows.append(row)
assert len(rows)==20,len(rows)
for w in ('city','race'):
 b,f=counts[(w,'baseline')],counts[(w,'fast')]
 for k in ('byte','word','long','other_width','ROM','WRAM','POLY','IO_bucket','other_bucket'):assert b[k]==f[k],(w,k)
 assert int(b['callbacks'])-int(f['callbacks'])==int(f['hits'])
keys=list(dict.fromkeys(k for r in rows for k in r))
with (root/'comparison.tsv').open('w') as f:
 writer=csv.DictWriter(f,keys,delimiter='\t');writer.writeheader();writer.writerows(rows)
print('workload mode median_sim_Hz median_render_FPS resyncs discarded_ms sim_Hz_range')
for w in ('city','race'):
 for mode in ('baseline','fast'):
  rs=[r for r in rows if r['workload']==w and r['phase']=='performance' and r['mode']==mode];assert len(rs)==3
  hz=[float(r['sim_hz']) for r in rs]
  print(w,mode,*(f'{statistics.median(float(r[k]) for r in rs):.3f}' for k in ('sim_hz','render_fps','resyncs','discarded_lag_ms')),f'{min(hz):.3f}..{max(hz):.3f}')
print('State, board/video digests, widths/regions and callback savings validated:',root/'comparison.tsv')
