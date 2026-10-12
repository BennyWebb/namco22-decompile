#!/usr/bin/env python3
"""Validate the 16-run dense-opening projection comparison and report medians."""
import csv,json,pathlib,re,statistics,sys
root=pathlib.Path(sys.argv[1])
def fields(line):return dict(re.findall(r'(\w+)=([^ ]+)',line))
def read(name, directory=root):
    p=directory/name
    assert p.with_suffix('.exit').read_text().strip()=='0',name
    state=json.loads(p.with_suffix('.json').read_text())
    assert state['traps']==state['unmapped']==state['romwrites']==0
    assert not state['dsp_fault'] and not state['sound_fault']
    result={'state':state}
    for line in p.with_suffix('.log').read_text().splitlines():
        for prefix,key in [('[SIM-RATE]','rate'),('[PROFILE] total','profile'),
             ('[PROFILE-CACHE] total','cache'),('[MEMORY]','memory'),
             ('[GEOMETRY-RATE]','lag'),('[BOARD-STATE]','board'),
             ('[GEOMETRY-PROJECTION]','projection')]:
            if line.startswith(prefix):result[key]=fields(line)
    return result
runs={}
for trial in range(1,6):
    for flag in ('OFF','ON'):
        name=f'dense-{trial}-{flag}';runs[name]=read(name)
for prefix in ('fixed-1','fixed-2','detail'):
    rows=[]
    for flag in ('OFF','ON'):
        name=f'{prefix}-{flag}';r=runs[name]=read(name)
        data=list(csv.DictReader((root/(name+'.csv')).open()))
        assert len(data)==301 and sum(d['rendered']=='1' for d in data)==100
        selected=[d for d in data if d['rendered']=='1']
        r['cpu_ms']={k:1000*statistics.mean(float(d[k]) for d in selected)
          for k in ('scene_cpu','list_stage_cpu','sort_stage_cpu','backend_inclusive_cpu','submit_inclusive_cpu','vertex_cpu','swap_cpu')}
        r['presentation_cpu_ms']=1000*statistics.mean(sum(float(d[k]) for k in
            ('scene_cpu','backend_inclusive_cpu','swap_cpu')) for d in selected)
        for d in data:
            assert abs(float(d['backend_inclusive_cpu'])-sum(float(d[k]) for k in
                ('backend_cpu','lookup_cpu','decode_cpu','upload_cpu','submit_cpu','vertex_cpu','draw_cpu')))<1e-7
        rows.append(data)
    for a,b in zip(*rows):
        for key in ('frame','rendered','quads','triangles','draws','vbo_uploads','vbo_bytes','hits','misses','evictions','texture_uploads','binds','sort_moved_bytes','sort_allocations'):
            assert a[key]==b[key],(prefix,a['frame'],key)
reference=next(iter(runs.values()))
assert all(r['state']==reference['state'] and r['board']==reference['board'] for r in runs.values())
(root/'summary.json').write_text(json.dumps(runs,indent=2)+'\n')
for flag in ('OFF','ON'):
    group=[runs[f'dense-{t}-{flag}'] for t in range(1,6)]
    print('projection',flag)
    for category,key in [('rate','sim_hz'),('rate','render_fps'),('profile','scene'),('profile','submission'),('profile','avg_draws'),('profile','avg_triangles'),('profile','avg_binds'),('memory','RSS_kib')]:
        values=[float(r[category][key]) for r in group]
        print(f' {category}.{key} median={statistics.median(values):.6f} range={min(values):.6f}..{max(values):.6f}')
    fixed=[runs[f'fixed-{t}-{flag}'] for t in (1,2)]
    print(' fixed presentation CPU ms',statistics.mean(r['presentation_cpu_ms'] for r in fixed))
    for k in fixed[0]['cpu_ms']:print(' ',k,statistics.mean(r['cpu_ms'][k] for r in fixed))
    print(' detail',runs[f'detail-{flag}'].get('projection'))
paired=[100*(float(runs[f'dense-{t}-ON']['rate']['render_fps'])/float(runs[f'dense-{t}-OFF']['rate']['render_fps'])-1) for t in range(1,6)]
print('Paired FPS changes %:',paired,'median',statistics.median(paired))
print('All 16 exits, state/board hashes and fixed-frame geometry/cache/VBO checks passed')

# Optional separately collected coarse-only scopes (no geometry instrumentation).
if len(sys.argv)>2:
    directory=pathlib.Path(sys.argv[2]);coarse={}
    for trial in (1,2):
        rows=[]
        for flag in ('OFF','ON'):
            name=f'fixed-{trial}-{flag}';r=coarse[name]=read(name,directory)
            assert r['state']==reference['state'] and r['board']==reference['board']
            data=list(csv.DictReader((directory/(name+'.csv')).open()))
            assert len(data)==301 and sum(d['rendered']=='1' for d in data)==100
            selected=[d for d in data if d['rendered']=='1']
            r['cpu_ms']={k:1000*statistics.mean(float(d[k]) for d in selected)
              for k in ('scene_cpu','list_stage_cpu','sort_stage_cpu','backend_inclusive_cpu','submit_inclusive_cpu','vertex_cpu','swap_cpu')}
            r['presentation_cpu_ms']=1000*statistics.mean(sum(float(d[k]) for k in ('scene_cpu','backend_inclusive_cpu','swap_cpu')) for d in selected)
            for d in data:
                assert abs(float(d['backend_inclusive_cpu'])-sum(float(d[k]) for k in ('backend_cpu','lookup_cpu','decode_cpu','upload_cpu','submit_cpu','vertex_cpu','draw_cpu')))<1e-7
            rows.append(data)
        for a,b in zip(*rows):
            for key in ('frame','rendered','quads','triangles','draws','vbo_uploads','vbo_bytes','hits','misses','evictions','texture_uploads','binds','sort_moved_bytes','sort_allocations'):
                assert a[key]==b[key],(trial,a['frame'],key)
    (directory/'summary.json').write_text(json.dumps(coarse,indent=2)+'\n')
    for flag in ('OFF','ON'):
        group=[coarse[f'fixed-{t}-{flag}'] for t in (1,2)]
        print('Coarse-only',flag,'presentation CPU ms',statistics.mean(r['presentation_cpu_ms'] for r in group))
        for k in group[0]['cpu_ms']:print(' ',k,statistics.mean(r['cpu_ms'][k] for r in group))
    print('All four coarse-only state/board/geometry/cache/VBO and scope checks passed')
