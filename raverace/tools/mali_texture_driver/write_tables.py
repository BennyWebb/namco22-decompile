"""Generate report tables from accepted normal replay data; no inferred GPU time."""
from pathlib import Path
import json,statistics,sys
root=Path(sys.argv[1]);s=json.loads((root/'summary.json').read_text());d=json.loads((root/'details.json').read_text());m=s['medians'];v=['baseline','original','candidate'];names=['Production','Original arrays','Retained arrays']
lines=['| Metric: median of five accepted racing windows | '+' | '.join(names)+' |','|---|---:|---:|---:|']
metrics=[('Rendered FPS','fps'),('1% low FPS','low1_fps'),('p99 presentation interval ms','frame_ms_p99'),('Worst presentation interval ms','frame_ms_max'),('Image age median ms','age_ms_median'),('Image age p95 ms','age_ms_p95'),('GLES-owner CPU ms/picture','presentation_cpu_ms'),('Submission wall ms/picture','submit_ms'),('Allocation/upload/retirement scope ms/picture','upload_ms'),('Decode wall ms/picture','decode_ms'),('World draws/picture','draws'),('World/cache binds/picture','binds'),('Peak process RSS MiB','peak_rss_mib'),('Simulation Hz','sim_hz')]
for label,key in metrics:lines.append('| '+label+' | '+' | '.join(f'{m[x][key]:.3f}' for x in v)+' |')
lines+=['','Normal benchmarks retain the established combined texture scope. Per-API','allocation/upload separation is reported only in the controlled diagnostics;','those independent timings are not distributed into this table.','','| Lifetime counter / allocation | Production | Original arrays | Retained arrays |','|---|---:|---:|---:|']
for label,key,scale in [('Physical array peak MiB','peak_bytes',1048576),('Physical array end MiB','physical_bytes',1048576),('End logical-payload occupancy %','logical_payload_occupancy_percent',1),('Cohorts allocated, whole process','allocations',1),('Existing-cohort matches, whole process','hole_reuses',1)]:
 values=['64.00' if key in ['peak_bytes','physical_bytes'] else '—']+[f'{d["medians"][x]["lifetime"][key]/scale:.2f}' for x in ['original','candidate']];lines.append('| '+label+' | '+' | '.join(values)+' |')
lines+=['','The existing-cohort counter includes filling a virgin second layer, reused','holes, and (for retention) completed empty cohorts; it is not a count solely','of overwritten layers. Peak/end array figures include charged retired/idle','storage. Driver-private allocations are not included.','','| Round | Production FPS / 1% low / worst ms | Original FPS / 1% low / worst ms | Retained FPS / 1% low / worst ms |','|---|---:|---:|---:|']
for pair in [1,3,4,5,6]:
 values=[]
 for variant in v:
  r=next(r for r in s['runs'] if r['variant']==variant and r['pair']==pair)['whole'];values.append(f'{r["fps"]:.2f} / {r["low1_fps"]:.2f} / {r["frame_ms_max"]:.2f}')
 lines.append('| '+str(pair)+' | '+' | '.join(values)+' |')
lines+=['','| Approximately five-second section | Production FPS / 1% low / worst ms | Original FPS / 1% low / worst ms | Retained FPS / 1% low / worst ms |','|---|---:|---:|---:|']
for section in range(13):
 values=[]
 for variant in v:
  rs=[r['sections'][section] for r in s['runs'] if r['variant']==variant];values.append(' / '.join(f'{statistics.median(r[k] for r in rs):.2f}' for k in ['fps','low1_fps','frame_ms_max']))
 lines.append('| '+s['runs'][0]['sections'][section]['frames']+' | '+' | '.join(values)+' |')
lines+=['','| Tick-1965 burst: median of five | Production | Original arrays | Retained arrays |','|---|---:|---:|---:|']
for label,key in [('Misses','misses'),('Upload/lifecycle scope ms','upload_wall'),('Eviction scope ms','eviction_wall'),('Decode ms','decode_wall'),('Submission ms','submission_wall'),('Owner CPU ms','cpu'),('Presentation interval ms','interval_ms')]:lines.append('| '+label+' | '+' | '.join(f'{d["medians"][x]["burst"][key]:.3f}' for x in v)+' |')
(root/'report-tables.md').write_text('\n'.join(lines)+'\n')
