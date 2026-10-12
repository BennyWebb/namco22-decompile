#!/usr/bin/env python3
"""Analyse saved audit logs/CSV without plotting libraries or ROMs.

Usage: analyse_architecture_audit.py RESULTS_DIRECTORY [SDK_LIBC]
Writes summary.json and comparison.tsv. CPU columns are main-thread CPU only;
GPU elapsed spans include idle gaps, not utilisation. Missing metrics stay null.
"""
import bisect,collections,csv,json,math,pathlib,re,statistics,subprocess,sys
root=pathlib.Path(sys.argv[1])
def fields(line): return dict(re.findall(r'(\w+)=([^ ]+)',line))
def distribution(values):
    if not values:return None
    a=sorted(values)
    return {'mean':statistics.mean(a),'median':statistics.median(a),
            'p95':a[max(0,math.ceil(.95*len(a))-1)],'max':a[-1],'n':len(a)}
def cohort(rows):
    result={'n':len(rows)}
    for key in rows[0] if rows else []:
        if key not in ('frame','rendered','time_s'):
            result[key]=distribution([float(r[key]) for r in rows])
    for kind in ('wall','cpu'):
        result['presentation_'+kind]=distribution([sum(float(r[k]) for k in
            ('scene_'+kind,'backend_inclusive_'+kind,'swap_'+kind)) for r in rows])
        # Host preparation plus initial backend state calls, excluding upload
        # and ordered submission (including vertex/draw), and excluding swap.
        result['preparation_'+kind]=distribution([float(r['scene_'+kind])+
            float(r['backend_inclusive_'+kind])-float(r['submit_inclusive_'+kind])-
            float(r['upload_inclusive_'+kind]) for r in rows])
    return result
def telemetry(path,start,end):
    rows=[json.loads(l) for l in path.read_text().splitlines()] if path.exists() else []
    correlation=root/'legacy-clock-correlation.json'
    legacy=json.loads(correlation.read_text()) if correlation.exists() else None
    for row in rows:
        if 'raw_time' in row:row['time']=row['raw_time']
        elif legacy:row['time']+=legacy['raw_minus_monotonic_s']
    selected=[r for r in rows if start<=r['time']<=end]
    result={'samples':len(selected),'alignment':'contemporaneous RAW' if rows and 'raw_time' in rows[0]
        else 'approximate post-run offset' if legacy else 'legacy uncorrected MONOTONIC'}
    for key in ('cpu_khz','gpu_hz'):
        result[key]=dict(collections.Counter(r[key] for r in selected))
    result['gpu_debug_util_percent']=distribution([float(m[1]) for r in selected
        if (m:=re.search(r'Utilisation from last show:(\d+)%;',r.get('gpu_debug') or ''))])
    result['temperatures_C']={name:distribution([float(r['thermal'][name]['temp'])/1000
        for r in selected if r['thermal'].get(name,{}).get('temp')])
        for name in {n for r in selected for n in r['thermal']}}
    for label,text,key in [('rss_kib','status','VmRSS'),('available_kib','memory','MemAvailable')]:
        result[label]=distribution([float(m[1]) for r in selected
            if (m:=re.search(r'^'+key+r':\s+(\d+)',r.get(text) or '',re.M))])
    # Delta counters between telemetry samples whose endpoints are in range.
    cpu_times=collections.defaultdict(float);core_times=collections.defaultdict(lambda:[0,0]);duration=0
    migrations=collections.Counter()
    for a,b in zip(selected,selected[1:]):
        dt=b['time']-a['time'];duration+=dt
        for tid,t in b['threads'].items():
            if tid in a['threads']:
                old=a['threads'][tid];cpu_times[t['name']]+=((t['utime']+t['stime'])-(old['utime']+old['stime']))/b['hz']
                migrations[t['name']]+=t['cpu']!=old['cpu']
        for core,v in b['cores'].items():
            if core in a['cores']:
                d=[x-y for x,y in zip(v,a['cores'][core])];total=sum(d[:8]);idle=sum(d[3:5]);core_times[core][0]+=total-idle;core_times[core][1]+=total
    result['covered_seconds']=duration
    result['thread_one_core_percent']={k:100*v/duration for k,v in cpu_times.items()} if duration else {}
    result['observed_cpu_migrations']=dict(migrations)
    result['core_busy_percent']={k:100*v[0]/v[1] for k,v in core_times.items() if v[1]}
    return result
runs={}
for path in sorted(root.glob('*.log')):
    name=path.stem;text=path.read_text();lines=text.splitlines()
    rate=next((fields(l) for l in lines if l.startswith('[SIM-RATE]')),None)
    if not rate:continue
    r={'rate':rate,'stage':{},'profile':{},'cohorts':{},'pmu':{}}
    for line in lines:
        f=fields(line)
        if line.startswith('[SIM-STAGE]'):r['stage'][f['name']]=f
        elif line.startswith('[GEOMETRY-RATE]'):r['resync']=f
        elif line.startswith('[MEMORY]'):r['memory']=f
        elif line.startswith('[TEX] decoded='):r['texture']=f
        elif line.startswith('[PROFILE] total '):r['profile']=f
        elif line.startswith('[PROFILE-CACHE] total '):r['cache']=f
        elif line.startswith('[PMU] name='):r['pmu'][f['name']]=f
        elif line.startswith('[GPU-AUDIT] valid='):r['gpu_status']=f
    csvpath=root/(name+'.csv')
    if csvpath.exists():
        rows=list(csv.DictReader(csvpath.open()))
        r['cohorts']={k:cohort([x for x in rows if x['rendered']==v]) for k,v in [('rendered','1'),('skipped','0'),('all',None)]}
        r['cohorts']['all']=cohort(rows)
        r['subranges']={f'{lo}-{hi}':{'all':cohort([x for x in rows if lo<=int(x['frame'])<=hi]),
            'rendered':cohort([x for x in rows if lo<=int(x['frame'])<=hi and x['rendered']=='1'])}
            for lo,hi in [(1400,1700),(1701,2200),(2201,2700),(2701,3200),(3201,3600)]
            if any(lo<=int(x['frame'])<=hi for x in rows)}
        r['telemetry']=telemetry(root/(name+'-system.jsonl'),float(rows[0]['time_s'])-float(rows[0]['sim_wall']),float(rows[-1]['time_s']))
        # Exclusive nested scopes must sum to their inclusive parent.
        categories=['backend','lookup','decode','upload','submit','vertex','draw']
        for kind in ('wall','cpu'):
            r['reconciliation_'+kind+'_max_us']=max(abs(float(x['backend_inclusive_'+kind])-sum(float(x[s+'_'+kind]) for s in categories))*1e6 for x in rows)
        gpath=root/(name+'.csv.gpu.csv')
        if gpath.exists():
            g=list(csv.DictReader(gpath.open()));valid=[x for x in g if x['disjoint']=='0']
            r['gpu_elapsed_ms']=distribution([float(x['elapsed_ns'])/1e6 for x in valid]);r['gpu_result_age_ticks']=distribution([float(x['result_age_ticks']) for x in valid])
            r['gpu_disjoint_rows']=len(g)-len(valid)
    runs[name]=r
(root/'summary.json').write_text(json.dumps(runs,indent=2)+'\n')
headers=['run','simulation_hz','render_fps','renders','skipped','resyncs','discarded_ms','simulation_avg_ms','simulation_p95_ms','scene_ms','lookup_decode_ms','upload_ms','draw_ms','swap_ms','draws','triangles','gpu_span_ms','rss_kib']
with (root/'comparison.tsv').open('w') as out:
    w=csv.writer(out,delimiter='\t');w.writerow(headers)
    for name,r in runs.items():
        rate=r['rate'];stage=r['stage'].get('simulation_wall',{});p=r['profile'];res=r.get('resync',{})
        w.writerow([name,rate.get('sim_hz'),rate.get('render_fps'),rate.get('rendered'),rate.get('skipped'),res.get('resyncs'),res.get('discarded_lag_ms'),stage.get('avg_ms'),stage.get('p95_ms'),p.get('scene'),p.get('lookup_decode'),p.get('uploads'),p.get('draw_call'),p.get('swap'),p.get('avg_draws'),p.get('avg_triangles'),(r.get('gpu_elapsed_ms') or {}).get('mean'),r.get('memory',{}).get('RSS_kib')])
for workload in ('city','race'):
    modes=sorted({n.split('-')[1] for n in runs if n.startswith(workload+'-')})
    for mode in modes:
        selected=[r for n,r in runs.items() if n.startswith(workload+'-'+mode+'-')]
        hz=[float(r['rate']['sim_hz']) for r in selected];fps=[float(r['rate']['render_fps']) for r in selected]
        print(workload,mode,'n='+str(len(selected)),f'sim median={statistics.median(hz):.3f} [{min(hz):.3f},{max(hz):.3f}]',f'FPS median={statistics.median(fps):.3f} [{min(fps):.3f},{max(fps):.3f}]')

# Optional private libc symbols: reject a different library before attributing
# stripped implementation PCs. No nearest-export guesses or assumed build IDs.
if len(sys.argv)>2:
    sdk=pathlib.Path(sys.argv[2]);device=root/'libc-device.so.6'
    def text_section(path):
        table=subprocess.check_output(['readelf','-WS',str(path)],text=True)
        m=re.search(r'\.text\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)',table)
        assert m, 'Missing libc text section'
        address,offset,size=(int(x,16) for x in m.groups())
        return address,path.read_bytes()[offset:offset+size]
    assert text_section(sdk)==text_section(device),'Device libc text/addresses differ from SDK; do not symbolise with it'
    symbols=[]
    for line in subprocess.check_output(['nm','-n','-S','--defined-only',str(sdk)],text=True).splitlines():
        p=line.split()
        if len(p)==4 and p[2] in 'tT':symbols.append((int(p[0],16),int(p[1],16),p[3]))
    starts=[s[0] for s in symbols];answer={}
    for workload in ('city','race'):
        counts=collections.Counter();total=0
        for file in root.glob(workload+'-profile-*-samples.tsv'):
            maps=[]
            for line in pathlib.Path(str(file)+'.maps').read_text().splitlines():
                p=line.split()
                if len(p)>=6 and 'libc.so' in p[-1]:
                    lo,hi=(int(x,16) for x in p[0].split('-'));maps.append((lo,hi,int(p[2],16)))
            for row in csv.DictReader(file.open(),delimiter='\t'):
                total+=1;pc=int(row['native_pc'],16)
                mapping=next((m for m in maps if m[0]<=pc<m[1]),None)
                if mapping:
                    address=pc-(mapping[0]-mapping[2]);n=bisect.bisect_right(starts,address)-1
                    name=symbols[n][2] if n>=0 and address<symbols[n][0]+symbols[n][1] else '[unresolved]'
                    counts[(row['stage'],name)]+=1
        answer[workload]={'all_main_thread_samples':total,'libc_text_verified':True,
            'symbols':[{'stage':stage,'function':name,'samples':n,'percent':100*n/total}
                for (stage,name),n in counts.most_common()] if total else []}
    (root/'libc-symbol-samples.json').write_text(json.dumps(answer,indent=2)+'\n')
