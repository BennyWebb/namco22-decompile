#!/usr/bin/env python3
"""Optional audit must preserve real game state, PCM/board/video and same-GPU pixels.
Usage: test_architecture_audit.py PLAIN AUDIT HEADLESS ROMS OUTPUT
Native: SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0
"""
import csv,json,pathlib,subprocess,sys
plain,audit,headless,roms,out=(str(pathlib.Path(p).resolve()) for p in sys.argv[1:6])
root=pathlib.Path(out);root.mkdir(parents=True,exist_ok=True)
for frame in (600,1700,3600):
    script=['--frames',str(frame),'--coin','180','--gas','200','--steer','300:2304','--steer','700:2048']
    ref=subprocess.run([headless,roms,*script],capture_output=True,text=True,check=True)
    snapshots=[]
    for mode,binary,flags in [('plain',plain,[]),('audit',audit,['--profile-stages','--audit-csv',str(root/f'{frame}.csv'),'--audit-gpu-timers'])]:
        capture=root/f'{frame}-{mode}.ppm'
        r=subprocess.run([binary,roms,*script,'--timing','independent','--test-present-every','3',
            '--skip-geometry-on-dropped-frames','on','--texture-budget-mib','64','--vbo-upload','frame',
            '--profile-from','100','--audio-checksum-from','1','--capture-frame',str(frame),'--capture',str(capture),*flags],
            capture_output=True,text=True,timeout=240)
        (root/f'{frame}-{mode}.log').write_text(r.stderr);(root/f'{frame}-{mode}.json').write_text(r.stdout)
        assert r.returncode==0,r.stderr
        assert json.loads(r.stdout)==json.loads(ref.stdout)
        lines=r.stderr.splitlines()
        digests=(next(l for l in lines if l.startswith('[AUDIO-CHECK]')),
            next(l for l in lines if l.startswith('[BOARD-STATE]')),
            next(l.split(' ticks=')[1] for l in lines if l.startswith('[GEOMETRY]')))
        snapshots.append((capture.read_bytes(),digests))
    assert snapshots[0]==snapshots[1],frame
    rows=list(csv.DictReader((root/f'{frame}.csv').open()));assert len(rows)==frame-99
    for row in rows:
        for kind in ('wall','cpu'):
            assert abs(float(row['backend_inclusive_'+kind])-sum(float(row[s+'_'+kind])
                for s in ['backend','lookup','decode','upload','submit','vertex','draw']))<1e-7
        if row['rendered']=='0':assert float(row['backend_inclusive_wall'])==0
    print(frame,'state, PCM/chip/board/video digests and pixels identical; CPU/wall scopes reconcile',flush=True)
