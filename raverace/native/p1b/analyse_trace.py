#!/usr/bin/env python3
"""Separate strace attribution; never merge tracer timings into headline metrics."""
import argparse,pathlib,re,json,csv
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run;s=json.loads((b/'p1b-summary.json').read_text());log=(b/'run.txt').read_text();offset=s['realtime_minus_monotonic'];start=s['native_begin_monotonic']+offset;end=s['native_end_monotonic']+offset
roles={}
for role,pattern in [('main',r'\[THREAD\] simulation_tid=(\d+)'),('prepare',r'\[PREP\] tid=(\d+)'),('present',r'\[THREAD\] presentation_tid=(\d+)')]:
 m=re.search(pattern,log);assert m;roles[int(m[1])]=role
records=[];unfinished=[]
for path in sorted(b.glob('syscalls.*')):
 tid=int(path.suffix[1:]);pending=None
 for line in path.read_text(errors='replace').splitlines():
  m=re.match(r'([\d.]+) (.*)',line)
  if not m:continue
  when=float(m[1]);text=m[2]
  if '<unfinished ...>' in text:pending=(when,text.replace('<unfinished ...>',''));continue
  if text.startswith('<... ') and pending:
   oldwhen,prefix=pending;pending=None;text=prefix+' '+text;when=oldwhen
  op=re.match(r'(\w+)\(',text)
  if not op:continue
  dur=re.search(r'<([\d.]+)>$',text);duration=float(dur[1]) if dur else 0
  if when+duration<start or when>end:continue
  records.append(dict(tid=tid,role=roles.get(tid,'other'),realtime=when,monotonic=when-offset,syscall=op[1],duration_ms=duration*1000,text=text))
 if pending:unfinished.append(dict(tid=tid,realtime=pending[0],text=pending[1]))
critical=[r for r in records if r['role']!='other'];writes=[r for r in critical if r['syscall'] in ['write','writev','pwrite64','fsync','fdatasync']];regular=[r for r in writes if re.search(r'\((?:1|2)(?:<|,)',r['text']) or any(x in r['text'] for x in ['/userdata/','/tmp/','/home/'])];opens=[r for r in critical if r['syscall']=='openat' and any(x in r['text'] for x in ['O_WRONLY','O_RDWR','O_CREAT'])]
for name,data in [('critical-writes.csv',writes),('slow-syscalls.csv',[r for r in records if r['duration_ms']>50]),('critical-file-opens.csv',opens)]:
 with (b/name).open('w') as f:
  w=csv.DictWriter(f,fieldnames=['tid','role','realtime','monotonic','syscall','duration_ms','text']);w.writeheader();w.writerows(data)
summary=dict(trace_only_not_performance=True,critical_threads=roles,native_realtime_window=[start,end],epoch_offset=offset,critical_write_syscalls=len(writes),critical_regular_or_stderr_writes=len(regular),critical_write_open_calls=len(opens),critical_write_records=writes,critical_write_open_records=opens,slow_syscalls_over50ms=[r for r in records if r['duration_ms']>50],unfinished_records=unfinished,trace_files=len(list(b.glob('syscalls.*'))))
(b/'trace-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('Critical regular/stderr writes:',len(regular),'all critical writes:',len(writes),'write-mode opens:',len(opens))
