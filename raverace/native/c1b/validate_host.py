#!/usr/bin/env python3
"""Separate unpaced host validation, retaining raw 36 pictures per path."""
import pathlib,os,json,subprocess,hashlib,csv,time
root=pathlib.Path(__file__).resolve().parents[3];private=root/'raverace/native/generated/c1b';coverage=root/'raverace/native/generated/milestone4a-coverage';exe=private/'b2-validation/host/rr_p1b'
for mode in ['rgba','index']:
 run=private/f'host-final-{mode}';run.mkdir();(run/'images').mkdir()
 env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','LP_NUM_THREADS':'1','ENG_P1_VALIDATION':'1','ENG_P1_QUEUE':'2','ENG_P1B_VALVE':'0','ENG_P3_PRIORITY':'off','ENG_GPU_HUD':'1','ENG_GPU_HUD_GEOMETRY':'1','ENG_LIVE_PACK':str(coverage/'race-live-v2'),'ENG_LIVE_LOOKUP':str(coverage/'lookup/live.lookup'),'ENG_LIVE_SHADERS':str(root/'raverace/native/generated/stage1b/hardware-shaders'),'ENG_LIVE_CAPTURE':'hash-only','ENG_P1B_WARM_PACKET':str(coverage/'commands/tick-1800.live'),'ENG_P1B_RECEIPTS':str(run/'command-sha256.csv'),'ENG_P1B_IMAGE_HASH':str(run/'image-sha256.csv'),'ENG_C1_TEXTURES':mode,'ENG_C1_ATLAS':str(private/'reference'),'ENG_C1_IMAGES':str(run/'images'),'ENG_C1_SEMANTIC':str(run/'semantic-sha256.csv')}
 for key in ['LD_PRELOAD','ENG_P1_FENCE','ENG_P1_TEST_FALLBACK','ENG_LIVE_COMMAND_DUMP']:env.pop(key,None)
 cmd=[str(exe),str(root/'raverace/extracted'),'--presentation-thread','on','--timing','independent','--replay-input',str(root/'raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec'),'--input-checkpoints',str(run/'states.jsonl'),'--audio','off','--thread-profile',str(run/'thread.csv')]
 (run/'method.json').write_text(json.dumps({'command':cmd,'mode':mode,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'validation_only':True},indent=2)+'\n')
 t=time.monotonic()
 with (run/'run.txt').open('w') as f:r=subprocess.run(cmd,env=env,cwd=root,stdout=f,stderr=subprocess.STDOUT)
 (run/'exit.txt').write_text(str(r.returncode)+'\n');print(mode,'exit',r.returncode,'seconds',time.monotonic()-t,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
reference=root/'raverace/benchmarks/native-p1-20261015/validation-q3'
read=lambda p:[json.loads(l) for l in p.read_text().splitlines()]
for mode in ['rgba','index']:assert read(private/f'host-final-{mode}/states.jsonl')==read(reference/'states.jsonl')
assert (private/'host-final-rgba/command-sha256.csv').read_bytes()==(reference/'command-sha256.csv').read_bytes()
assert (private/'host-final-index/semantic-sha256.csv').read_bytes()==(private/'host-final-rgba/semantic-sha256.csv').read_bytes()
assert len(list(csv.DictReader((private/'host-final-index/semantic-sha256.csv').open())))==4207
print('Checkpoint and semantic-command parity PASS; images require separate analysis.',flush=True)
