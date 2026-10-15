#!/usr/bin/env python3
"""Full host semantic validation, isolated from H700 performance measurements."""
import argparse, os, pathlib, subprocess, json, hashlib, tarfile
p=argparse.ArgumentParser();p.add_argument('variant',choices=['O1','O2','O2-nosp']);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[4]
private=root/'raverace/native/generated/stage1b'
run=private/'host-validation'/a.variant;run.mkdir(parents=True,exist_ok=False)
shaders=private/'hardware-shaders'
if not shaders.exists():
    shaders.mkdir()
    with tarfile.open(root/'raverace/native/generated/milestone4a-hardware/bundle.tar') as t:
        for m in t.getmembers():
            if '/shaders/' in '/'+m.name and m.isfile():
                (shaders/pathlib.Path(m.name).name).write_bytes(t.extractfile(m).read())
assert list(shaders.glob('*.frag')), 'Missing original hardware shaders'
coverage=root/'raverace/native/generated/milestone4a-coverage'
recording=root/'raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec'
(run/'commands').mkdir()
env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy',
     'LP_NUM_THREADS':'1','ENG_GPU_HUD':'1','ENG_GPU_HUD_GEOMETRY':'1',
     'ENG_LIVE_PACK':str(coverage/'race-live-v2'),'ENG_NATIVE_ASSET_DIR':str(coverage/'race-live-v2'),
     'ENG_LIVE_SHADERS':str(shaders),'ENG_LIVE_LOOKUP':str(coverage/'lookup/live.lookup'),
     'ENG_LIVE_COMMAND_DUMP':str(run/'commands'),'ENG_STAGE1_DETAIL':'0',
     'ENG_STAGE1_CSV':str(run/'audit.csv')}
for key in ['LD_PRELOAD','RR_AFFINITY','ENG_LIVE_CAPTURE','ENG_LIVE_TELEMETRY','ENG_LIVE_SIM_TELEMETRY']:
    env.pop(key,None)
cmd=[str(private/'host'/a.variant/'rr_native_flags'),str(root/'raverace/extracted'),
     '--replay-input',str(recording),'--input-checkpoints',str(run/'states.jsonl'),
     '--presentation-thread','off','--audio','off']
(run/'method.json').write_text(json.dumps(dict(command=cmd,validation_only=True,environment={k:v for k,v in env.items() if k.startswith(('SDL_','ENG_','LP_'))}),indent=2)+'\n')
with (run/'run.txt').open('w') as log:
    result=subprocess.run(cmd,env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT)
(run/'exit.txt').write_text(str(result.returncode)+'\n')
subprocess.run(['python3',str(pathlib.Path(__file__).with_name('validate_flags.py')),str(run),str(recording)+'.states.jsonl',str(coverage/'commands')],check=True)
print(run)
