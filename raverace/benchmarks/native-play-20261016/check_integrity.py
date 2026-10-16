"""Host SSH read-only integrity capture; writes results on the host only."""
import pathlib,subprocess,json,shlex
b=pathlib.Path(__file__).parent
code='''import pathlib,subprocess,json,hashlib,datetime
app=pathlib.Path('/userdata/system/rave-racer-native-g2'); manifest='/userdata/system/rave-racer-native-g2-20261016/protected.sha256'
p=subprocess.run(['sha256sum','-c',manifest],capture_output=True,text=True)
prod=hashlib.sha256(pathlib.Path('/userdata/roms/ports/rave-gles2/rr_gles2').read_bytes()).hexdigest()
j=json.loads((app/'clock-journal.json').read_text());current={k:pathlib.Path(k).read_text().strip() for k in j['original']}
hashes={str(p.relative_to(app)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (app/'sessions/20261016-225025-27784-297185128').iterdir() if p.is_file()}
hashes.update({n:hashlib.sha256((app/n).read_bytes()).hexdigest() for n in ['latest-summary.txt','latest-summary.json','clock-journal.json']})
print(json.dumps(dict(checked_at=datetime.datetime.now().astimezone().isoformat(),production_sha256=prod,protected_manifest=manifest,protected_returncode=p.returncode,protected_results=p.stdout.splitlines(),protected_stderr=p.stderr,clock_journal_restored=j['restored'],current_clocks=current,clock_values_match_original=current==j['original'],game_pids=subprocess.run(['pidof','rr_native'],capture_output=True,text=True).stdout.strip(),emulationstation_pids=subprocess.run(['pidof','emulationstation'],capture_output=True,text=True).stdout.strip(),retrieved_hashes=hashes,native_eeprom_sha256=hashlib.sha256(pathlib.Path('/userdata/saves/rave-racer-native/rr_eeprom.nv').read_bytes()).hexdigest()),indent=2))'''
s=subprocess.check_output(['ssh','-F','/dev/null','-S','/tmp/rr-play-master','-o','BatchMode=yes','root@192.168.1.69','python3 -B -c '+shlex.quote(code)],text=True);j=json.loads(s);(b/'production-integrity.json').write_text(s)
assert j['production_sha256']=='6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e'
assert j['protected_returncode']==0 and len(j['protected_results'])==47 and all(x.endswith(': OK') for x in j['protected_results'])
assert j['clock_journal_restored'] and j['clock_values_match_original'] and not j['game_pids'] and j['emulationstation_pids']
local=json.loads((b/'retrieved-sha256.json').read_text())
for k,h in j['retrieved_hashes'].items():
 name=k
 assert local[name]==h,(name,h)
print('PASS production hash +47protected paths;restored clocks;ES/no game;all copied telemetry hashes exact')
