#!/usr/bin/env python3
"""Read-only final check on the handheld; expected app manifest supplied as argv1."""
import pathlib,json,hashlib,subprocess
app=pathlib.Path('/userdata/system/rave-racer-native-g2');b=pathlib.Path('/userdata/system/rave-racer-native-g2-20261016')
expected=json.loads(pathlib.Path(__import__('sys').argv[1]).read_text())
for name,h in expected.items():assert hashlib.sha256((app/name).read_bytes()).hexdigest()==h,name
p=subprocess.run(['sha256sum','-c',str(b/'protected.sha256')],capture_output=True,text=True);assert p.returncode==0 and len(p.stdout.splitlines())==47
j=json.loads((app/'latest-summary.json').read_text());assert j['exit_code']==0 and not j['clock_restore_errors'];assert j['smoke_start_select_seen'];assert j['processed_pictures']>6500 and not j['unmatched_records'];assert not j['dropped_fallback_key_records'];assert all('dropped_records=0' in s and 'errors=0' in s for s in j['io_receipts']) and j['io_receipts']
assert not(app/'active').exists();assert json.loads((app/'clock-journal.json').read_text())['restored'];assert subprocess.run(['pidof','rr_native'],capture_output=True).returncode!=0
es=subprocess.check_output(['pidof','emulationstation'],text=True).strip();assert es
for i in range(4):
 root=pathlib.Path(f'/sys/devices/system/cpu/cpu{i}/cpufreq');assert (root/'scaling_governor').read_text().strip()=='schedutil';assert (root/'scaling_min_freq').read_text().strip()=='480000';assert (root/'scaling_max_freq').read_text().strip()=='1512000'
assert pathlib.Path('/sys/class/devfreq/gpu/governor').read_text().strip()=='simple_ondemand'
assert pathlib.Path('/userdata/roms/ports/Rave Racer (Native).sh').exists();assert pathlib.Path('/userdata/saves/rave-racer-native/rr_eeprom.nv').stat().st_size==8192
out=dict(pass_checks=True,app_files=len(expected),protected_paths=47,emulationstation_pid=es,production_sha256=hashlib.sha256(pathlib.Path('/userdata/roms/ports/rave-gles2/rr_gles2').read_bytes()).hexdigest(),summary=j)
(b/'final-verification.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS',len(expected),'app hashes;47 protected paths;clock restore;ES;START+SELECT;complete telemetry')
