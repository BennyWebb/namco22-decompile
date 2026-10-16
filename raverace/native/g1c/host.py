#!/usr/bin/env python3
import pathlib,os,subprocess,json,argparse,time
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--play',action='store_true');p.add_argument('--observe-cache',action='store_true');p.add_argument('--keys',action='store_true');p.add_argument('--audit',action='store_true');p.add_argument('--attract',action='store_true');p.add_argument('--frames',type=int);p.add_argument('--debug',action='store_true');p.add_argument('--stream-trace',action='store_true');a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/g1c';run=base/a.name;run.mkdir();(run/'images').mkdir();old=root/'raverace/native/generated/c1e';cov=root/'raverace/native/generated/milestone4a-coverage';pack=cov/'race-live-v2' if a.audit else pathlib.Path((base/'selected.txt').read_text().strip());atlas=old/'axis/index' if a.audit else pack
keys={'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','LP_NUM_THREADS':'1','ENG_P1_VALIDATION':'1','ENG_P1_QUEUE':'2','ENG_P1B_VALVE':'0','ENG_P3_PRIORITY':'off','ENG_GPU_HUD':'1','ENG_GPU_HUD_GEOMETRY':'1','ENG_LIVE_PACK':str(pack),'ENG_LIVE_LOOKUP':str(cov/'lookup/live.lookup' if a.audit else pack/'live.lookup'),'ENG_LIVE_SHADERS':str(root/'raverace/native/generated/stage1b/hardware-shaders'),'ENG_P1B_RECEIPTS':str(run/'command-sha256.csv'),'ENG_C1_TEXTURES':'index','ENG_C1_ATLAS':str(atlas),'ENG_C1_SAMPLER':'texture'}
if a.keys:keys['ENG_G1C_KEYS']='1'
if a.stream_trace:keys['ENG_G1B_STREAM_TRACE']='1'
if a.audit:keys['ENG_G1_STATE_AUDIT']='1'
if not a.attract and not a.frames:keys.update(ENG_LIVE_CAPTURE='hash-only',ENG_P1B_IMAGE_HASH=str(run/'image-sha256.csv'),ENG_C1_IMAGES=str(run/'images'),ENG_C1_SEMANTIC=str(run/'semantic-sha256.csv'),ENG_G1_REFERENCE_IDENTITY=str(base/'reference-identity.bin'))
if a.debug:keys.update(ENG_C1_DEBUG='provenance',ENG_C1_PROVENANCE_DIR=str(run/'images'))
env={k:v for k,v in os.environ.items() if not k.startswith(('ENG_','RR_','SND_'))};env.update(keys);cmd=[str(root/'raverace/native/generated/g1c-observe/b2-play/host/rr_p1b' if a.observe_cache else base/('b2-play/host/rr_p1b' if a.play else 'b2-validation/host/rr_p1b')),str(root/'raverace/extracted'),'--presentation-thread','on','--timing','independent','--audio','off','--thread-profile',str(run/'thread.csv'),'--input-checkpoints',str(run/'states.jsonl')]
if not a.attract:cmd+=['--replay-input',str(root/'raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec')]
if a.frames:cmd+=['--frames',str(a.frames)]
(run/'method.json').write_text(json.dumps({'cmd':cmd,'env':keys},indent=2));t=time.monotonic()
with (run/'run.txt').open('w') as f, (run/'stdout.txt').open('w') as out:r=subprocess.run(cmd,env=env,stdout=out if a.attract else f,stderr=f)
(run/'exit.txt').write_text(str(r.returncode)+'\n');print(a.name,r.returncode,'seconds',time.monotonic()-t,flush=True)
