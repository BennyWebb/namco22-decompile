#!/usr/bin/env python3
import pathlib,os,json,subprocess,hashlib,csv,time
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1c';cov=root/'raverace/native/generated/milestone4a-coverage';exe=base/'b2-validation/host/rr_p1b';reference=root/'raverace/native/generated/c1b/host-final-rgba'
for sampler,precision in [('texture','highp'),('texture','lowp')]:
 run=base/f'{sampler}-{precision}';run.mkdir();(run/'images').mkdir()
 env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','LP_NUM_THREADS':'1','ENG_P1_VALIDATION':'1','ENG_P1_QUEUE':'2','ENG_P1B_VALVE':'0','ENG_P3_PRIORITY':'off','ENG_GPU_HUD':'1','ENG_GPU_HUD_GEOMETRY':'1','ENG_LIVE_PACK':str(cov/'race-live-v2'),'ENG_LIVE_LOOKUP':str(cov/'lookup/live.lookup'),'ENG_LIVE_SHADERS':str(root/'raverace/native/generated/stage1b/hardware-shaders'),'ENG_LIVE_CAPTURE':'hash-only','ENG_P1B_WARM_PACKET':str(cov/'commands/tick-1800.live'),'ENG_P1B_RECEIPTS':str(run/'command-sha256.csv'),'ENG_P1B_IMAGE_HASH':str(run/'image-sha256.csv'),'ENG_C1_TEXTURES':'index','ENG_C1_ATLAS':str(base/'compact'),'ENG_C1_IMAGES':str(run/'images'),'ENG_C1_SEMANTIC':str(run/'semantic-sha256.csv'),'ENG_C1_SAMPLER':sampler,'ENG_C1_PALETTE_PRECISION':precision}
 for key in ['LD_PRELOAD','ENG_P1_FENCE','ENG_P1_TEST_FALLBACK','ENG_LIVE_COMMAND_DUMP']:env.pop(key,None)
 cmd=[str(exe),str(root/'raverace/extracted'),'--presentation-thread','on','--timing','independent','--replay-input',str(root/'raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec'),'--input-checkpoints',str(run/'states.jsonl'),'--audio','off','--thread-profile',str(run/'thread.csv')]
 (run/'method.json').write_text(json.dumps({'command':cmd,'sampler':sampler,'precision':precision,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'host_only':True},indent=2)+'\n')
 t=time.monotonic()
 with (run/'run.txt').open('w') as f:r=subprocess.run(cmd,env=env,cwd=root,stdout=f,stderr=subprocess.STDOUT)
 (run/'exit.txt').write_text(str(r.returncode)+'\n');assert r.returncode==0
 assert (run/'states.jsonl').read_bytes()==(reference/'states.jsonl').read_bytes()
 assert (run/'semantic-sha256.csv').read_bytes()==(reference/'semantic-sha256.csv').read_bytes()
 assert len(list(csv.DictReader((run/'image-sha256.csv').open())))==36
 print(run.name,'PASS states/commands; seconds',time.monotonic()-t,flush=True)
