"""Bounded per-state/course distributions and rolling diagnostic logs."""
import pathlib,json,math,re,csv,collections,array,time,hashlib,os
T=1/59.7104
class Rolling:
 def __init__(self,path,limit=2*1024*1024):self.path=path;self.limit=limit;self.size=0;self.rotations=0;self.f=path.open('wb',buffering=65536)
 def write(self,data):
  if self.size+len(data)>self.limit:
   self.f.close();old=self.path.with_name(self.path.name+'.1');os.replace(self.path,old);self.f=self.path.open('wb',buffering=65536);self.size=0;self.rotations+=1
  self.f.write(data[:self.limit]);self.size+=min(len(data),self.limit)
 def close(self):self.f.close()
class Group:
 def __init__(self):
  self.native_repeats=self.native_gaps=self.native_settled=0;self.native=self.legacy=self.repeats=self.underruns=self.native_audio=self.gaps=self.misses=self.evictions=self.copied=0;self.hist=array.array('I',[0])*10002;self.n=0;self.prep_worst=0;self.reasons=collections.Counter();self.resources={};self.clock_flags=0
 def add(self,row,cov,prev,release):
  tick=int(row['frame']);native=int(cov['native']);self.native+=native;self.legacy+=not native
  if not native:self.reasons[cov['reason']]+=1
  ms=float(row['prep_wall'])*1000;self.n+=1;self.hist[min(10001,math.ceil(ms*100))]+=1;self.prep_worst=max(self.prep_worst,ms);event={}
  if prev:
   interval=float(row['presented'])-float(prev['presented']);repeat=max(0,math.floor(interval/T+.5)-1);under=int(row['audio_underruns'])-int(prev['audio_underruns']);gap=float(row['presented'])-float(row['publication_age'])-float(prev['presented'])+float(prev['publication_age']);self.repeats+=repeat;self.native_audio+=under if native else 0;self.native_repeats+=repeat if native else 0
   if release is not None and tick>release:self.underruns+=under;self.native_settled+=under if native else 0
   if gap>.05:self.gaps+=1;self.native_gaps+=1 if native else 0
   if repeat or gap>.05 or under:event=dict(tick=tick,native=native,repeats=repeat,underrun_frames=under,main_gap_ms=gap*1000,prepare_ms=ms,compiler_ms=float(row['native_compile_ms']),simulation_ms=float(row['simulation_ms']),swap_ms=float(row['swap_wall'])*1000)
  return event or None
 def summary(self):
  rank=math.ceil(self.n*.99);acc=0;p99=None
  for i,n in enumerate(self.hist):
   acc+=n
   if rank and acc>=rank:p99='>100ms' if i==10001 else i/100;break
  return dict(native=self.native,legacy=self.legacy,repeats=self.repeats,native_repeats=self.native_repeats,native_main_gaps_over50ms=self.native_gaps,settled_native_underrun_frames=self.native_settled,settled_underrun_frames=self.underruns,all_native_underrun_frames=self.native_audio,main_gaps_over50ms=self.gaps,preparation_p99_ms_upper_0_01=p99,preparation_worst_ms=self.prep_worst,fallback_reasons=dict(self.reasons),dynamic_misses=self.misses,dynamic_evictions=self.evictions,staging_copied_bytes=self.copied,resources=self.resources,clock_flagged_samples=self.clock_flags)
class Collector:
 def __init__(self,run,save):
  self.run=run;self.save=save;self.logs={n:Rolling(run/(n+'.log')) for n in ['stderr','stdout','profile','resources','problems']};self.groups={};self.pending={};self.next=1;self.prev=None;self.header=None;self.current='unknown';self.release=None;self.early=[];self.events=collections.deque(maxlen=256);self.fallbacks=collections.Counter();self.dropped_keys=self.unmatched=self.save_writes=0;self.io=[];self.smoke_chord=False;self.samples=0
 def group(self,key):
  if key not in self.groups:
   if len(self.groups)>=127:key='overflow'
   if key not in self.groups:self.groups[key]=Group()
  return self.groups[key]
 def problem(self,obj):self.events.append(obj);self.logs['problems'].write((json.dumps(obj)+'\n').encode())
 def line(self,kind,line):
  if kind=='stderr' and line.startswith('[G2-EEPROM] '):
   data=bytes.fromhex(line.split(' ',1)[1].strip());assert len(data)==8192;temp=self.save.with_suffix('.new')
   with temp.open('wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
   os.replace(temp,self.save);self.save_writes+=1;line='[G2-EEPROM] persisted bytes=8192 sha256='+hashlib.sha256(data).hexdigest()+'\n'
  self.logs[kind].write(line.encode(errors='replace'))
  if kind=='profile':
   values=next(csv.reader([line]))
   if values and values[0]=='frame':self.header=values;return
   if not self.header or len(values)!=len(self.header):self.unmatched+=1;return
   row=dict(zip(self.header,values));self.pending.setdefault(int(row['frame']),{})['row']=row
  elif kind=='stderr':
   m=re.search(r'\[G1-COVERAGE\] tick=(\d+) main=(\d+) sub=(\d+) screen=(\d+) course=(\d+) major=(\d+) walk=\d+ direct=\d+ native=(\d+) reason=(.*)',line)
   if m:
    t,main,sub,screen,route,course,native,reason=m.groups();key=f'main={main} sub={sub} screen={screen} course={course}';self.current=key;self.pending.setdefault(int(t),{})['cov']=dict(key=key,native=int(native),reason=reason[:256])
   m=re.search(r'P1B-RELEASE\] completed_tick=(\d+)',line)
   if m:
    self.release=int(m[1])
    for t,key,n,native in self.early:
     if t>self.release:self.group(key).underruns+=n;self.group(key).native_settled+=n if native else 0
    self.early=[]
   m=re.search(r'G2-STAGING\] tick=(\d+) copied_bytes=(\d+)',line)
   if m:self.pending.setdefault(int(m[1]),{})['copied']=int(m[2])
   m=re.search(r'G1B-PREP\] tick=(\d+) hits=\d+ misses=(\d+) evictions=(\d+)',line)
   if m:self.pending.setdefault(int(m[1]),{})['dynamic']=(int(m[2]),int(m[3]))
   if '[G1B-FALLBACK]' in line:
    key=line.split('[G1B-FALLBACK] ',1)[1].strip();key=re.sub(r'^tick=\d+ ','',key)
    if key in self.fallbacks or len(self.fallbacks)<4096:self.fallbacks[key]+=1
    else:self.dropped_keys+=1
    self.problem(dict(fallback_exact=line.strip()))
   if '[P1B-IO]' in line:self.io.append(line.strip())
   if '[G2-SMOKE] START+SELECT' in line:self.smoke_chord=True
  while self.next in self.pending and 'row' in self.pending[self.next] and 'cov' in self.pending[self.next]:
   p=self.pending.pop(self.next);row=p['row'];cov=p['cov'];g=self.group(cov['key']);g.copied+=p.get('copied',0);d=p.get('dynamic',(0,0));g.misses+=d[0];g.evictions+=d[1];event=g.add(row,cov,self.prev,self.release)
   if self.release is None and self.prev:
    if len(self.early)<8192:self.early.append((self.next,cov['key'],int(row['audio_underruns'])-int(self.prev['audio_underruns']),cov['native']))
    else:self.unmatched+=1
   if event:self.problem(dict(state=cov['key'],**event))
   self.prev=row;self.next+=1
  if len(self.pending)>8192:
   first=min(self.pending);self.pending.pop(first);self.unmatched+=1
   if first==self.next:self.next+=1
 def sample(self,pid):
  self.samples+=1;g=self.group(self.current);v={};root=pathlib.Path('/sys')
  try:
   status=pathlib.Path(f'/proc/{pid}/status').read_text();mem=pathlib.Path('/proc/meminfo').read_text()
   for key,text in [('VmRSS',status),('VmHWM',status),('MemAvailable',mem),('CmaFree',mem)]:
    m=re.search(r'^'+key+r':\s+(\d+)',text,re.M)
    if m:v[key+'_KiB']=int(m[1])
   mali=(root/'kernel/debug/mali0/gpu_memory').read_text();m=re.search(r'^mali0\s+(\d+)',mali,re.M)
   if m:v['Mali_pages']=int(m[1])
   for i in range(4):v[f'cpu{i}_khz']=int((root/f'devices/system/cpu/cpu{i}/cpufreq/scaling_cur_freq').read_text())
   v['gpu_hz']=int((root/'class/devfreq/gpu/cur_freq').read_text())
   for p in (root/'class/thermal').glob('thermal_zone*/temp'):v[p.parent.name+'_mC']=int(p.read_text())
   for p in (root/'class/thermal').glob('cooling_device*/cur_state'):v[p.parent.name]=int(p.read_text())
   if any(v.get(f'cpu{i}_khz')!=1512000 for i in range(4)) or v.get('gpu_hz')!=240000000 or any(v.get('cooling_device'+str(i),0) for i in range(2)):g.clock_flags+=1
  except (OSError,ValueError):v['read_error']=1
  for key,value in v.items():
   item=g.resources.setdefault(key,dict(min=value,max=value));item['min']=min(item['min'],value);item['max']=max(item['max'],value)
  if self.samples%10==0:self.logs['resources'].write((json.dumps(dict(monotonic=time.monotonic(),state=self.current,values=v))+'\n').encode())
 def finish(self,exit_code,restore_errors,extra):
  out=dict(exit_code=exit_code,clock_restore_errors=restore_errors,groups={k:g.summary() for k,g in self.groups.items()},legacy_release_tick=self.release,settled_audio_definition='All pictures after actual legacy release; native-only settled and all-native counters also reported',processed_pictures=self.next-1,unmatched_records=self.unmatched+len(self.pending),fallback_exact_keys=dict(self.fallbacks),dropped_fallback_key_records=self.dropped_keys,events=list(self.events),save_writes=self.save_writes,io_receipts=self.io,smoke_start_select_seen=self.smoke_chord,sample_count=self.samples,retention='Five log streams,2x2MiB each;128state groups;4096fallback keys;256recent problem events;8sessions. Prep p99 nearest-rank0.01ms upper bins,overflow>100ms.',**extra)
  text=['Rave Racer Native session',f'Exit: {exit_code}; clocks restored: {not restore_errors}',f'Pictures: {self.next-1}; incomplete telemetry records: {out["unmatched_records"]}',f'Native EEPROM writes: {self.save_writes}',f'Raw logs rotate; summaries retain all accumulated counts.']
  for key,g in out['groups'].items():text.append(f'{key}: native={g["native"]} legacy={g["legacy"]} repeats={g["repeats"]} settled_underruns={g["settled_underrun_frames"]} main_gaps50={g["main_gaps_over50ms"]} prep_p99_ms={g["preparation_p99_ms_upper_0_01"]}')
  text.append(f'Exact fallback keys: {len(self.fallbacks)}; unretained key records: {self.dropped_keys}; clock/cooling flags: {sum(g.clock_flags for g in self.groups.values())}')
  resources={}
  for g in self.groups.values():
   for key,v in g.resources.items():
    a=resources.setdefault(key,dict(v));a['min']=min(a['min'],v['min']);a['max']=max(a['max'],v['max'])
  text.append('Resource ranges (KiB/pages/mC as labelled): '+json.dumps(resources,sort_keys=True))
  text.extend('Fallback: '+key+' count='+str(n) for key,n in self.fallbacks.items())
  text.extend(self.io)
  (self.run/'summary.json').write_text(json.dumps(out,indent=2)+'\n');(self.run/'summary.txt').write_text('\n'.join(text)+'\n')
  for f in self.logs.values():f.close()
  return out
