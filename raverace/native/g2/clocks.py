"""Session-only clock policy, with independent watchdog and persisted originals."""
import pathlib,json,os,time,signal,sys

def atomic(path,obj):
 p=path.with_name(path.name+'.new');p.write_text(json.dumps(obj,indent=2)+'\n');os.replace(p,path)
def identity(pid):
 try:
  f=pathlib.Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split();return None if f[0]=='Z' else f[19]
 except (OSError,IndexError):return None
def snapshot(root=pathlib.Path('/sys')):
 paths=[root/f'devices/system/cpu/cpu{i}/cpufreq'/f for i in range(4) for f in ['scaling_governor','scaling_min_freq','scaling_max_freq']]+[root/'class/devfreq/gpu'/f for f in ['governor','min_freq','max_freq']];result={str(p):p.read_text().strip() for p in paths}
 for i in range(4):
  p=root/f'devices/system/cpu/cpu{i}/cpufreq'
  if result[str(p/'scaling_governor')]=='userspace':result[str(p/'scaling_setspeed')]=(p/'scaling_setspeed').read_text().strip()
 return result
def apply(root=pathlib.Path('/sys')):
 for i in range(4):
  p=root/f'devices/system/cpu/cpu{i}/cpufreq';assert 'userspace' in (p/'scaling_available_governors').read_text();(p/'scaling_governor').write_text('userspace');(p/'scaling_setspeed').write_text('1512000')
 p=root/'class/devfreq/gpu';(p/'min_freq').write_text('0');(p/'max_freq').write_text('0');(p/'governor').write_text('simple_ondemand')
def restore(original):
 errors=[]
 # Restore frequency bounds before the governor, matching the validated runners.
 for ending in ['max_freq','scaling_max_freq','min_freq','scaling_min_freq','governor','scaling_governor','scaling_setspeed']:
  for key,value in original.items():
   if pathlib.Path(key).name==ending:
    try:pathlib.Path(key).write_text(value)
    except OSError as e:errors.append(f'{key}: {e}')
 for key,value in original.items():
  try:
   if pathlib.Path(key).read_text().strip()!=value:errors.append(f'{key}: restoration mismatch')
  except OSError as e:errors.append(str(e))
 return errors
def low_priority():
 try:os.sched_setscheduler(0,os.SCHED_IDLE,os.sched_param(0))
 except OSError:pass
 try:os.nice(19)
 except OSError:pass
def watchdog(path):
 low_priority();j=json.loads(path.read_text());path.with_name('watchdog.ready').write_text(str(os.getpid()))
 while True:
  j=json.loads(path.read_text())
  if j.get('restored'):return
  if identity(j['owner'])!=j['owner_start']:break
  time.sleep(.5)
 pid=j.get('game');start=j.get('game_start')
 if pid and identity(pid)==start:
  os.kill(pid,signal.SIGTERM);time.sleep(3)
  if identity(pid)==start:os.kill(pid,signal.SIGKILL)
 j['restore_errors']=restore(j['original']);j['restored']=not j['restore_errors'];j['watchdog_recovered']=True;atomic(path,j)
if __name__=='__main__':watchdog(pathlib.Path(sys.argv[1]))
