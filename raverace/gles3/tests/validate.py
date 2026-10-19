#!/usr/bin/env python3
"""Exact local-ROM replay gates. Git contains hashes; replay payloads stay local."""
from pathlib import Path
import argparse,csv,hashlib,json,os,re,subprocess
FIX=Path(__file__).resolve().parent/'fixtures'
p=argparse.ArgumentParser();p.add_argument('case',choices=['reference','cone','attract','provenance','unsupported','sound']);p.add_argument('--build',type=Path,required=True);p.add_argument('--roms',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);p.add_argument('--recordings',type=Path,default=os.environ.get('GLES3_TEST_RECORDINGS'));p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert a.recordings,'set GLES3_TEST_RECORDINGS or --recordings'
a.output.mkdir(parents=True,exist_ok=False);out=a.output.resolve();identities=json.loads((FIX/'recordings.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def recording(name):
 q=a.recordings/(name+'.rec');v=identities[name];assert sha(q)==v['input_sha256'];assert sha(Path(str(q)+'.eeprom'))==v['eeprom_sha256'];return q.resolve()
def run(case,binary,*,sound=False):
 d=out/case;d.mkdir();(d/'images').mkdir();env={k:v for k,v in os.environ.items() if not k.startswith(('RR_','SND_','ENG_'))};env.pop('LD_PRELOAD',None)
 env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='1',ENG_GLES3_HOST='1',ENG_GLES3_ASSETS=str(a.cache.resolve()),ENG_GLES3_SUMMARY_PATH=str(d/'latest-summary.txt'))
 mode=case.rsplit('-',1)[0] if sound else case;name='reference' if mode in ('provenance','unsupported') else mode
 cmd=[str(binary.resolve()),str(a.roms.resolve()),'--audio','off','--overlay','off','--input-checkpoints',str(d/'states.jsonl')]
 if name=='attract':cmd+=['--frames','36000']
 else:
  q=recording(name);cmd+=['--replay-input',str(q)];env['ENG_GLES3_INITIAL_EEPROM']=str(q)+'.eeprom'
 if sound:
  cmd+=['--audio-checksum-from','1'];env['ENG_SOUND_REFERENCE']=str(d/'audio.jsonl')
 else:
  cmd+=['--thread-profile',str(d/'thread.csv'),'--profile-simulation','--simulation-profile-from','1']
  env.update(ENG_GLES3_CAPTURE='hash-only',ENG_GLES3_IMAGES=str(d/'images'),ENG_GLES3_IMAGE_HASH=str(d/'images.csv'))
  if mode in ('reference','provenance'):env.update(ENG_GLES3_RECEIPTS=str(d/'commands.csv'),ENG_GLES3_NORMALIZED_RECEIPTS=str(d/'normalized.csv'),ENG_GLES3_SEMANTIC=str(d/'semantic.csv'))
  if mode=='cone':env['ENG_GLES3_CAPTURE_TICKS']='11598,11822,11823,11824,11825,11826,11827,11828,11936,12228,12500,12800,13225'
  if mode=='attract':env['ENG_GLES3_CAPTURE_TICKS']='1,159,435,3462,6006,12000,18000,24000,30000,36000'
  if mode=='provenance':env.update(ENG_GLES3_PROVENANCE='1',ENG_GLES3_PROVENANCE_DIR=str(d/'images'),ENG_GLES3_PALETTE_REFERENCE=str(d/'palettes.bin'))
  if mode=='unsupported':env['ENG_GLES3_FORCE_UNSUPPORTED']='20'
 (d/'method.json').write_text(json.dumps(dict(cmd=cmd,env={k:v for k,v in env.items() if k.startswith(('ENG_','SDL_','LP_'))},binary_sha256=sha(binary)),indent=2))
 with (d/'stdout.jsonl').open('w') as f,(d/'stderr.txt').open('w') as log:r=subprocess.run(cmd,env=env,stdout=f,stderr=log)
 (d/'exit.txt').write_text(str(r.returncode)+'\n');log=(d/'stderr.txt').read_text()
 if mode=='unsupported':
  assert r.returncode==86;message=(d/'latest-summary.txt').read_text().splitlines()[0];assert message.startswith('GLES3 UNSUPPORTED: tick=20 state=0/0 key=69/4660') and message in log;return d
 assert r.returncode==0,r.returncode
 assert '[SND] TRANSLATED DRIVER' not in log and 'GLES3 UNSUPPORTED' not in log
 records=[hashlib.sha256(s.encode()+b'\n').hexdigest() for s in (d/'states.jsonl').read_text().splitlines()]
 if mode in ('reference','provenance','cone','attract'):
  expected=json.loads((FIX/(mode+'-checkpoints.json')).read_text());assert records==[x['sha256'] for x in expected],('checkpoints',mode)
 if not sound:
  if mode in ('reference','provenance'):
   assert (d/'normalized.csv').read_bytes()==(FIX/'reference-commands.csv').read_bytes(),'commands'
   assert len(list(csv.DictReader((d/'normalized.csv').open())))==4207
   semantic=FIX/'reference-semantic.csv'
   assert (d/'semantic.csv').read_bytes()==semantic.read_bytes(),'semantic'
  if mode=='provenance':
   expected=json.loads((FIX/'provenance-images.json').read_text());assert {p.name:sha(p) for p in (d/'images').iterdir()}==expected,'provenance images'
  else:assert (d/'images.csv').read_bytes()==(FIX/(mode+'-images.csv')).read_bytes(),'images'
  if mode=='reference':
   boot=re.findall(r'GLES3-BOOT-ORACLE\] tick=(\d+).*differences=(\d+)',log);assert len(boot)==159 and all(int(x[1])==0 for x in boot)
 if mode=='cone':
  final=[json.loads(s) for s in (d/'stdout.jsonl').read_text().splitlines() if s.startswith('{"schema":')][-1]
  assert hashlib.sha256(json.dumps(final,sort_keys=True,separators=(',',':')).encode()).hexdigest()==(FIX/'cone-final.sha256').read_text().strip()
 return d
if a.case=='sound':
 results={}
 for name in ['reference','attract','cone','fixture','dense-1','dense-2','dense-3']:
  paths=[run(name+'-'+tag,a.build/'gles3'/binary,sound=True) for tag,binary in [('translated','rr_gles3'),('oracle','rr_gles3_sound_oracle')]]
  def audio(d):return [json.loads(s) for s in (d/'audio.jsonl').read_text().splitlines() if s.startswith('{"audio_frame":')]
  x,y=map(audio,paths);assert x==y and x,'PCM/voices';assert (paths[0]/'states.jsonl').read_bytes()==(paths[1]/'states.jsonl').read_bytes()
  if name=='reference':assert x==[json.loads(s) for s in (FIX/'sound-reference.jsonl').read_text().splitlines()]
  results[name]=len(x);print('EXACT SOUND',name,len(x),flush=True)
 (out/'result.json').write_text(json.dumps(results,indent=2))
else:
 d=run(a.case,a.build/'gles3/rr_gles3_validation');(out/'result.json').write_text(json.dumps(dict(case=a.case,exact=True)));print('EXACT',a.case,flush=True)
