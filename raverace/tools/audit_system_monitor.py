#!/usr/bin/env python3
"""One-second read-only device telemetry; no governor/thermal/driver changes."""
import json,os,pathlib,sys,time
pid=int(sys.argv[1]);out=pathlib.Path(sys.argv[2])
def read(path):
 try:return pathlib.Path(path).read_text().strip()
 except (OSError,ValueError):return None
def tasks():
 result={}
 for path in pathlib.Path('/proc/%d/task'%pid).glob('*/stat'):
  s=read(path)
  if not s:continue
  end=s.rfind(')');parts=s[end+2:].split()
  result[path.parent.name]={'name':s[s.find('(')+1:end], 'utime':int(parts[11]),'stime':int(parts[12]),'cpu':int(parts[36]),'schedstat':read(path.parent/'schedstat')}
 return result
with out.open('w') as f:
 while pathlib.Path('/proc/%d'%pid).exists():
  cpu=read('/proc/stat')
  # SDL's Linux performance counter uses MONOTONIC_RAW. Record both domains
  # rather than assuming its absolute epoch equals Python's MONOTONIC.
  row={'time':time.monotonic(),'raw_time':time.clock_gettime(time.CLOCK_MONOTONIC_RAW),
       'pid':pid,'hz':os.sysconf('SC_CLK_TCK'),'threads':tasks()}
  row['cores']={line.split()[0]:list(map(int,line.split()[1:])) for line in (cpu or '').splitlines() if line.startswith('cpu') and line.split()[0]!='cpu'}
  for name,path in [('cpu_khz','/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq'),('gpu_hz','/sys/class/devfreq/gpu/cur_freq'),('gpu_debug','/sys/kernel/debug/sunxi_gpu/dump'),('memory','/proc/meminfo'),('status','/proc/%d/status'%pid)]:row[name]=read(path)
  row['thermal']={p.name:{'type':read(p/'type'),'temp':read(p/'temp')} for p in pathlib.Path('/sys/class/thermal').glob('thermal_zone*')}
  f.write(json.dumps(row)+'\n');f.flush();time.sleep(1)
