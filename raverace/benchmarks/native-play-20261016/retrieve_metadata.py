"""Host-only SSH reader: no commands in this script write handheld files."""
import pathlib,subprocess,json
out=pathlib.Path(__file__).parent
code='''import pathlib,json,hashlib,datetime
b=pathlib.Path('/userdata/system/rave-racer-native-g2');result={}
for d in sorted((b/'sessions').iterdir()):
 result[d.name]={p.name:dict(bytes=p.stat().st_size,mtime_ns=p.stat().st_mtime_ns) for p in d.iterdir() if p.is_file()}
nv=pathlib.Path('/userdata/saves/rave-racer-native/rr_eeprom.nv')
print(json.dumps(dict(retrieved_at=datetime.datetime.now().astimezone().isoformat(),sessions=result,native_eeprom_sha256=hashlib.sha256(nv.read_bytes()).hexdigest()),indent=2))'''
s=subprocess.check_output(['ssh','-F','/dev/null','-S','/tmp/rr-play-master','-o','BatchMode=yes','root@192.168.1.69','python3 -c '+__import__('shlex').quote(code)],text=True)
json.loads(s);(out/'device-inventory.json').write_text(s)
