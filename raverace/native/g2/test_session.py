"""Host-only tests: fake sysfs clocks, crash watchdog, telemetry and isolated saves."""
import pathlib,tempfile,sys,subprocess,time,json,os,signal,io,shutil
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import clocks,session,assets
from telemetry import Collector
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);root=p/'sys'
 for i in range(4):
  x=root/f'devices/system/cpu/cpu{i}/cpufreq';x.mkdir(parents=True)
  for n,v in dict(scaling_governor='schedutil',scaling_min_freq='480000',scaling_max_freq='1512000',scaling_setspeed='0',scaling_available_governors='userspace schedutil').items():(x/n).write_text(v)
 x=root/'class/devfreq/gpu';x.mkdir(parents=True)
 for n,v in dict(governor='simple_ondemand',min_freq='0',max_freq='0').items():(x/n).write_text(v)
 original=clocks.snapshot(root);clocks.apply(root);assert (root/'devices/system/cpu/cpu0/cpufreq/scaling_governor').read_text()=='userspace';assert not clocks.restore(original);assert clocks.snapshot(root)==original
 owner=subprocess.Popen(['sleep','60']);game=subprocess.Popen(['sleep','60']);journal=p/'clock-journal.json';clocks.atomic(journal,dict(owner=owner.pid,owner_start=clocks.identity(owner.pid),game=game.pid,game_start=clocks.identity(game.pid),original=original,restored=False));clocks.apply(root)
 watch=subprocess.Popen([sys.executable,str(pathlib.Path(clocks.__file__)),str(journal)])
 for _ in range(100):
  if (p/'watchdog.ready').exists():break
  time.sleep(.02)
 assert (p/'watchdog.ready').exists();owner.kill();owner.wait();watch.wait(timeout=8);game.wait(timeout=2);j=json.loads(journal.read_text());assert j['restored'] and j['watchdog_recovered'] and clocks.snapshot(root)==original
 prod=p/'prod';(prod/'extracted').mkdir(parents=True);seed=bytes(range(256))*32;(prod/'extracted/rv1eeprm.9e').write_bytes(seed);(prod/'controller-input.cfg').write_text('original input');save=p/'save';nv=session.prepare_save(p,prod,save);assert nv.read_bytes()==seed;nv.write_bytes(seed[::-1]);session.prepare_save(p,prod,save);assert nv.read_bytes()==seed[::-1] and (prod/'extracted/rv1eeprm.9e').read_bytes()==seed
 run=p/'run';run.mkdir();c=Collector(run,nv);c.line('stderr','[G2-EEPROM] '+seed.hex()+'\n');assert nv.read_bytes()==seed
 c.line('profile','frame,prep_wall,presented,publication_age,audio_underruns,native_compile_ms,simulation_ms,swap_wall\n')
 for tick in range(1,4):
  c.line('stderr',f'[G1-COVERAGE] tick={tick} main=1 sub=2 screen=3 course=4 major=5 walk=0 direct=0 native=1 reason=ok\n');c.line('profile',f'{tick},.008,{tick/59.7104},.033,0,5,2,.008\n')
 c.line('stderr','[P1B-RELEASE] completed_tick=1\n');o=c.finish(0,[],{});assert o['processed_pictures']==3 and not o['unmatched_records'];assert sum(g['repeats'] for g in o['groups'].values())==0
 print('PASS fake-sysfs normal/owner-SIGKILL restoration; game termination; copied save isolation; EEPROM transport; telemetry join/pacing')
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);app=p/'app';app.mkdir();prod=p/'prod';(prod/'extracted').mkdir(parents=True);(prod/'extracted/rv1eeprm.9e').write_bytes(bytes(8192));save=p/'save';shutil.copyfile(clocks.__file__,app/'clocks.py')
 fake=app/'rr_native';fake.write_text('#!/usr/bin/python3\nimport os,sys\np=sys.argv[sys.argv.index("--thread-profile")+1]\nwith open(p,"w") as f:f.write("frame,prep_wall,presented,publication_age,audio_underruns,native_compile_ms,simulation_ms,swap_wall\\n1,.008,.0167,.03,0,5,2,.008\\n")\nprint("[G1-COVERAGE] tick=1 main=1 sub=0 screen=0 course=0 major=0 walk=0 direct=0 native=1 reason=covered",file=sys.stderr)\n');fake.chmod(0o755)
 clocks.snapshot=lambda:{};clocks.apply=lambda:None;assets.ensure=lambda *a:(app,False)
 assert session.run(app,prod,save)==0;assert not(app/'active').exists();j=json.loads((app/'latest-summary.json').read_text());assert j['processed_pictures']==1 and not j['unmatched_records']
 clocks.restore=lambda original:['injected restoration failure']
 assert session.run(app,prod,save)==91 and(app/'active').exists()
 print('PASS supervisor real pipe transport/normal cleanup; failed restoration returns91 and preserves recovery journal/lock')
