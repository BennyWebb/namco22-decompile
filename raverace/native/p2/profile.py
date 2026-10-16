#!/usr/bin/env python3
"""Offline symbolication, sample-based top tables and SVG flame graphs.
No profile-run timings are used as performance results.
"""
import argparse,pathlib,subprocess,csv,re,json,collections,html,hashlib,bisect
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('step');p.add_argument('--initial',action='store_true');p.add_argument('--executable',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];b=a.run;gen=root/'raverace/native/generated/p2';symfs=gen/'symfs';remote=pathlib.Path('userdata/system/rave-racer-native-p2-20261016');symdir=symfs/remote;symdir.mkdir(parents=True,exist_ok=True)
source=a.executable or (gen/'initial-builds'/(a.step+'-play-profile')/'rr_p1b' if a.initial else gen/(a.step+'-play-profile')/'arm/rr_p1b')
expected,remote_path=(b/'binary.sha256').read_text().splitlines()[0].split()
link=symdir/pathlib.Path(remote_path).name
if link.is_symlink():link.unlink()
if not link.exists():link.symlink_to(source.resolve())
assert expected==hashlib.sha256(link.read_bytes()).hexdigest(), 'Profile binary identity mismatch'
perf='/usr/lib/linux-tools/6.18.54.1-microsoft-standard-WSL2/bin/perf'
with (b/'perf-script.txt').open('w') as f,(b/'symbolication.txt').open('w') as err:
 result=subprocess.run([perf,'script','-i',str(b/'perf.data'),'--symfs',str(symfs),'--show-mmap-events','-F','comm,pid,tid,time,event,ip,sym,dso'],stdout=f,stderr=err)
assert result.returncode==0
rows={int(x['frame']):x for x in csv.DictReader((b/'thread.csv').open())}
assert sorted(rows)==list(range(1,6007)) and (b/'exit.txt').read_text().strip()=='0'
ref=root/'raverace/benchmarks/native-p1-20261015/validation-q3/states.jsonl'
assert [json.loads(x) for x in (b/'states.jsonl').read_text().splitlines()]==[json.loads(x) for x in ref.read_text().splitlines()]
io=re.search(r'\[P1B-IO\].*dropped_records=(\d+).*errors=(\d+)',(b/'run.txt').read_text());assert io and io[1]==io[2]=='0'
def bounds(t):
 r=rows[t];pub=float(r['presented'])-float(r['publication_age']);end=pub+float(r['prep_queue_age']);return end-float(r['prep_wall']),end
windows={'native':(bounds(1800)[0],bounds(6006)[1]),'dense':(bounds(3550)[0],bounds(3590)[1])};samples=[];current=None
for line in (b/'perf-script.txt').open():
 header=re.search(r'^\s*rr-prepare\s+.*?\s([\d]+\.[\d]+):',line)
 if header:
  if current:samples.append(current)
  current=dict(time=float(header[1]),stack=[]);continue
 if current:
  frame=re.match(r'\s+([0-9a-fA-F]+)\s+(.+?)\s+\((.+)\)\s*$',line)
  if frame:current['stack'].append((int(frame[1],16),frame[2],frame[3]))
if current:samples.append(current)
assert samples,'No rr-prepare samples decoded'
# Both available perf binaries lack libelf. Symbolicate exact IPs using the
# corresponding ELF symbol tables; never invent a name beyond its symbol span.
nm='/home/bwebb/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-nm'
symbols={};maps={}
for line in (b/'perf-script.txt').read_text().splitlines():
 m=re.search(r'PERF_RECORD_MMAP2 .*?: \[0x([0-9a-f]+)\(0x([0-9a-f]+)\) @ (0x[0-9a-f]+|0) .*?\]: [rwxps-]+ (.+)$',line)
 if m:maps.setdefault(m[4],[]).append((int(m[1],16),int(m[2],16),int(m[3],0)))
for sample in samples:
 resolved=[]
 for ip,name,dso in sample['stack']:
  path=symfs/dso.lstrip('/')
  if dso not in symbols:
   table=[]
   if path.is_file():
    args=[nm,'-n','-S','--defined-only',str(path)]
    result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    for line in result.stdout.splitlines():
     m=re.match(r'([0-9a-f]+) ([0-9a-f]+) ([TtWwiI]) (.+)',line)
     if m:table.append((int(m[1],16),int(m[2],16),m[4]))
   table.sort();symbols[dso]=(table,[x[0] for x in table])
  table,starts=symbols[dso];address=ip
  if '/lib/' in dso or '/usr/lib/' in dso:
   for begin,length,offset in maps.get(dso,[]):
    if begin<=ip<begin+length:address=ip-begin+offset;break
  i=bisect.bisect_right(starts,address)-1
  if i>=0 and address<table[i][0]+table[i][1]:name=table[i][2].split('@')[0]
  elif name=='[unknown]':name='[unknown:'+dso+']'
  resolved.append(name)
 sample['stack']=resolved

summary={};tables=[]
def flame(name,stacks):
 tree={'n':0,'children':{}}
 for stack,n in stacks.items():
  t=tree;t['n']+=n
  for f in stack:
   t=t['children'].setdefault(f,{'n':0,'children':{}});t['n']+=n
 depth=max((len(s) for s in stacks),default=1);height=(depth+2)*20;parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}">','<style>text{font:11px monospace}rect{stroke:white;stroke-width:.5}</style>',f'<text x="4" y="14">{html.escape(name)}; {tree["n"]} samples; attribution only</text>']
 def draw(t,x,width,d):
  left=x
  for label,c in sorted(t['children'].items()):
   w=width*c['n']/t['n'];y=height-(d+2)*20;shade=int(hashlib.sha256(label.encode()).hexdigest()[:2],16);color=f'rgb(245,{100+shade//3},{50+shade//5})';title=html.escape(label+f" — {c['n']} samples ({100*c['n']/tree['n']:.2f}%)");parts.append(f'<g><title>{title}</title><rect x="{left:.3f}" y="{y}" width="{w:.3f}" height="19" fill="{color}"/>')
   if w>30:parts.append(f'<text x="{left+3:.3f}" y="{y+13}">{html.escape(label[:max(0,int(w/7)-1)])}</text>')
   parts.append('</g>');draw(c,left,w,d+1);left+=w
 if tree['n']:draw(tree,0,1200,0)
 parts.append('</svg>');(b/(name+'.svg')).write_text('\n'.join(parts))
for window,(lo,hi) in windows.items():
 selected=[x for x in samples if lo<=x['time']<=hi];assert selected,window;stacks=collections.Counter();stages=collections.defaultdict(collections.Counter);selfs=collections.Counter();unknown=0
 for sample in selected:
  stack=sample['stack'] or ['[unknown]'];selfs[stack[0]]+=1;stacks[tuple(reversed(stack))]+=1
  stage='compiler' if any(re.search(r'\b(?:live_|p2_)',x) for x in stack) else 'walk' if any('eng_walk_list' in x for x in stack) else 'other'
  stages[stage][stack[0]]+=1;unknown+=stack[0].startswith('[unknown') or stack[0].startswith('0x')
 summary[window]=dict(window_monotonic=[lo,hi],samples=len(selected),unknown_self_samples=unknown,top10=[dict(function=f,samples=n,share=n/len(selected)) for f,n in selfs.most_common(10)],stages={stage:dict(samples=sum(c.values()),top10=[dict(function=f,samples=n,share=n/sum(c.values())) for f,n in c.most_common(10)]) for stage,c in stages.items()})
 for stage,c in [('all',selfs),*stages.items()]:
  for f,n in c.most_common():tables.append(dict(window=window,stage=stage,function=f,samples=n,share=n/sum(c.values())))
 flame('flame-'+window,stacks)
(b/'profile-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
with (b/'top-functions.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=['window','stage','function','samples','share']);w.writeheader();w.writerows(tables)
print(b.name,{w:{k:s[k] for k in ['samples','unknown_self_samples']} for w,s in summary.items()})
