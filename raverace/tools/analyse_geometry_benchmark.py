#!/usr/bin/env python3
"""Validate completed geometry factorial runs and summarise medians/CPU attribution.
Usage: analyse_geometry_benchmark.py RESULT_DIRECTORY
"""
import csv,json,pathlib,re,statistics,sys
root=pathlib.Path(sys.argv[1])
def fields(line):return dict(re.findall(r'(\w+)=([^ ]+)',line))
def read(name):
    p=root/name
    assert p.with_suffix('.exit').read_text().strip()=='0',name
    state=json.loads(p.with_suffix('.json').read_text())
    assert state['traps']==state['unmapped']==state['romwrites']==0,name
    assert not state['dsp_fault'] and not state['sound_fault'],name
    result={'state':state}
    for line in p.with_suffix('.log').read_text().splitlines():
        for prefix,key in [('[SIM-RATE]','rate'),('[PROFILE] total','profile'),
                           ('[PROFILE-CACHE] total','cache'),('[MEMORY]','memory'),
                           ('[GEOMETRY-RATE]','lag'),('[BOARD-STATE]','board')]:
            if line.startswith(prefix):result[key]=fields(line)
    return result
runs={}
for ordered in ('OFF','ON'):
    for indexed in ('OFF','ON'):
        for trial in range(1,6):
            name=f'dense-{trial}-ordered-{ordered}-index-{indexed}'
            runs[name]=read(name)
for trial in (1,2):
    for indexed in ('OFF','ON'):
        name=f'fixed-{trial}-index-{indexed}';runs[name]=read(name)
        rows=list(csv.DictReader((root/(name+'.csv')).open()))
        assert len(rows)==301 and sum(r['rendered']=='1' for r in rows)==100
        selected=[r for r in rows if r['rendered']=='1']
        runs[name]['cpu_ms']={k:1000*statistics.mean(float(r[k]) for r in selected)
            for k in ('scene_cpu','list_stage_cpu','sort_stage_cpu','sort_keys_cpu',
                      'sort_permutation_cpu','backend_inclusive_cpu','submit_inclusive_cpu',
                      'vertex_cpu','swap_cpu')}
        runs[name]['presentation_cpu_ms']=1000*statistics.mean(sum(float(r[k])
            for k in ('scene_cpu','backend_inclusive_cpu','swap_cpu')) for r in selected)
        for r in rows:
            assert abs(float(r['backend_inclusive_cpu'])-sum(float(r[k]) for k in
                ('backend_cpu','lookup_cpu','decode_cpu','upload_cpu','submit_cpu','vertex_cpu','draw_cpu')))<1e-7
reference=next(iter(runs.values()))
assert all(r['state']==reference['state'] and r['board']==reference['board'] for r in runs.values())
for trial in (1,2):
    rows=[list(csv.DictReader((root/f'fixed-{trial}-index-{v}.csv').open())) for v in ('OFF','ON')]
    for a,b in zip(*rows):
        for key in ('frame','rendered','quads','triangles','draws','vbo_uploads','vbo_bytes',
                    'hits','misses','evictions','texture_uploads','binds','sort_allocations'):
            assert a[key]==b[key],(trial,a['frame'],key)
    assert all(float(r['sort_moved_bytes'])==0 for r in rows[1])
(root/'summary.json').write_text(json.dumps(runs,indent=2)+'\n')
for ordered in ('OFF','ON'):
    values=[]
    for indexed in ('OFF','ON'):
        group=[runs[f'dense-{t}-ordered-{ordered}-index-{indexed}'] for t in range(1,6)]
        values.append(group)
        print('ordered',ordered,'index',indexed)
        for category,key in [('rate','sim_hz'),('rate','render_fps'),('profile','scene'),
            ('profile','submission'),('profile','avg_draws'),('profile','avg_triangles'),
            ('profile','avg_binds'),('cache','texture_uploads'),('memory','RSS_kib')]:
            a=[float(r[category][key]) for r in group]
            print(f'  {category}.{key}: median={statistics.median(a):.6f} range={min(a):.6f}..{max(a):.6f}')
    paired=[100*(float(b['rate']['render_fps'])/float(a['rate']['render_fps'])-1) for a,b in zip(*values)]
    print('  paired FPS %:',','.join(f'{x:.3f}' for x in paired),'median',statistics.median(paired))
for indexed in ('OFF','ON'):
    group=[runs[f'fixed-{t}-index-{indexed}'] for t in (1,2)]
    print('fixed index',indexed,'presentation CPU ms',statistics.mean(r['presentation_cpu_ms'] for r in group))
    for k in group[0]['cpu_ms']:print(' ',k,statistics.mean(r['cpu_ms'][k] for r in group))
print('All 24 exits, state/board hashes, CPU reconciliation and equivalent-frame geometry/cache/VBO checks passed')
