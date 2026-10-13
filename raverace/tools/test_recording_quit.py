#!/usr/bin/env python3
"""Native-only SDL quit injection: recording must end on a completed tick.
Uses real game boot/idle inputs, not a fabricated driving trajectory.
Usage: test_recording_quit.py GLES_BINARY HEADLESS_BINARY ROM_DIR
"""
import json
import os
import pathlib
import shlex
import subprocess
import sys
import tempfile
binary,headless,roms=[str(pathlib.Path(x).resolve()) for x in sys.argv[1:4]]
with tempfile.TemporaryDirectory(prefix='rr-record-quit-') as temp:
    root=pathlib.Path(temp)
    source=root/'quit.c'
    source.write_text('''#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <string.h>
int SDL_PollEvent(SDL_Event *event) {
 static unsigned calls;
 int (*real_poll)(SDL_Event *)=dlsym(RTLD_NEXT,"SDL_PollEvent");
 if(event && ++calls==200){memset(event,0,sizeof *event);event->type=SDL_QUIT;return 1;}
 return real_poll(event);
}
''')
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2'],text=True))
    library=root/'quit.so'
    subprocess.run(['cc','-shared','-fPIC',*flags,str(source),'-ldl','-o',str(library)],check=True)
    rec=root/'quit.rec'
    p=subprocess.run([binary,roms,'--interactive','--record-input',str(rec),'--audio','off'],
        env=dict(os.environ,LD_PRELOAD=str(library)),capture_output=True,text=True,timeout=60)
    assert p.returncode==0,p.stderr
    state=json.loads(p.stdout)
    assert state['frames']==int(pathlib.Path(str(rec)+'.end').read_text())
    q=subprocess.run([headless,roms,'--replay-input',str(rec),'--input-checkpoints',str(root/'play.states')],
        capture_output=True,text=True,timeout=60)
    assert q.returncode==0,q.stderr
    assert state==json.loads(q.stdout), 'Quit left a partial tick'
    assert pathlib.Path(str(rec)+'.states.jsonl').read_bytes()==(root/'play.states').read_bytes()
    print(f'SDL quit finalized tick {state["frames"]}; exact CPU/board/video/PCM/voice playback')
