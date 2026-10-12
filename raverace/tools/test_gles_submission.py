"""Compare ordered-submission builds on the same GPU, plus headless state.

Usage: test_ordered_submission.py BASELINE OPTIMIZED HEADLESS ROMS OUTPUT
"""
import pathlib,subprocess,json,sys,os
os.environ["GLES_TEXTURE_TRACE"]="1"
if len(sys.argv) != 6:
 raise SystemExit(__doc__)
base,opt,head,roms,out=sys.argv[1:]; out=pathlib.Path(out);out.mkdir(parents=True,exist_ok=True)
for f in (600,1700,3600):
 args=[roms,'--frames',str(f),'--coin','180','--gas','200','--steer','300:2304','--steer','700:2048']
 ref=subprocess.run([head,*args],capture_output=True,text=True,check=True)
 results=[]
 for name,exe in [('baseline',base),('optimized',opt)]:
  shot=out/f'{f}-{name}.ppm'
  p=subprocess.run([exe,*args,'--timing','independent','--test-present-every','3','--skip-geometry-on-dropped-frames','on','--texture-budget-mib','64','--vbo-upload','frame','--audio-checksum-from','1','--capture-frame',str(f),'--capture',str(shot)],capture_output=True,text=True,timeout=240)
  (out/f'{f}-{name}.log').write_text(p.stderr);(out/f'{f}-{name}.json').write_text(p.stdout)
  assert p.returncode==0,p.stderr
  assert json.loads(p.stdout)==json.loads(ref.stdout)
  digests=tuple(next(l for l in p.stderr.splitlines() if l.startswith(tag)) for tag in ('[AUDIO-CHECK]','[BOARD-STATE]','[GEOMETRY]','[TEXTURE-EVENTS]'))
  results.append((shot.read_bytes(),digests))
 assert results[0]==results[1],f
 print(f,'identical pixels, PCM/voice/board/video digests and headless state',flush=True)
