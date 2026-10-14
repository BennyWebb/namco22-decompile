#!/usr/bin/env python3
"""Build the frozen HUD probe, reusing the original CPU expansion verbatim.
Usage: build_gpu_hud_probe.py OUTPUT_DIRECTORY KNULLI_HOST
"""
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(sys.argv[1]).resolve()
host = pathlib.Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / 'raverace/src/rr_gl.c').read_text()
start = source.index('static bool build_text(')
end = source.index('\n#ifndef RR_GLES2\n/* *last:', start)
(out / 'hud_cpu_reference.inc').write_text(source[start:end])
sysroot = host / 'aarch64-buildroot-linux-gnu/sysroot'
subprocess.run([str(host / 'bin/aarch64-buildroot-linux-gnu-gcc'), '-O2',
                '-DRR_GLES2', '-DRR_GPU_HUD', '-I', str(root / 'raverace/include'),
                '-I', str(root / 'engine'), '-I', str(out),
                '-I', str(sysroot / 'usr/include/SDL2'),
                str(root / 'raverace/tools/gpu_hud_probe.c'), '-o', str(out / 'hud_probe'),
                '-lSDL2', '-lGLESv2', '-lmali', '-lm'], check=True)
