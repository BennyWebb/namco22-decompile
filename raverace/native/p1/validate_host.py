#!/usr/bin/env python3
import pathlib,subprocess,os,argparse,json,hashlib
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('queue',choices=['2','3']);p.add_argument('--fallback',type=int);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];private=root/'raverace/native/generated/p1';run=private/a.name;run.mkdir();(run/'commands').mkdir();(run/'captures').mkdir();coverage=root/'raverace/native/generated/milestone4a-coverage';recording=root/'raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec';exe=private/'host/rr_p1'
env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','LP_NUM_THREADS':'1','ENG_P1_VALIDATION':'1','ENG_P1_QUEUE':a.queue,'ENG_GPU_HUD':'1','ENG_GPU_HUD_GEOMETRY':'1','ENG_LIVE_PACK':str(coverage/'race-live-v2'),'ENG_LIVE_LOOKUP':str(coverage/'lookup/live.lookup'),'ENG_LIVE_SHADERS':str(root/'raverace/native/generated/stage1b/hardware-shaders'),'ENG_LIVE_COMMAND_DUMP':str(run/'commands'),'ENG_LIVE_CAPTURE':str(run/'captures')}
for key in ['LD_PRELOAD','ENG_P1_FENCE','ENG_P1_TEST_FALLBACK']:env.pop(key,None)
if a.fallback:env['ENG_P1_TEST_FALLBACK']=str(a.fallback)
cmd=[str(exe),str(root/'raverace/extracted'),'--replay-input',str(recording),'--input-checkpoints',str(run/'states.jsonl'),'--audio','off','--thread-profile',str(run/'thread.csv')]
(run/'method.json').write_text(json.dumps(dict(command=cmd,environment={k:v for k,v in env.items() if k.startswith(('SDL_','ENG_','LP_'))},binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),validation_only=True),indent=2)+'\n')
with (run/'run.txt').open('w') as log:result=subprocess.run(cmd,env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT)
(run/'exit.txt').write_text(str(result.returncode)+'\n')
print(run,result.returncode)
