"""Compile standalone diagnostics against the same Knulli toolchain/sysroot."""
from pathlib import Path
import shlex,subprocess
source=Path(__file__).resolve().parent
build=Path('/tmp/rr-world-array-arm-build');out=Path('/tmp/rr-mali-array');out.mkdir(exist_ok=True)
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text())
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
inc=shlex.split(flags.split('C_INCLUDES = ')[1].split('\n')[0])
for stem in ['probe','burst_probe']:
 subprocess.run([link[0],*[x for x in link if x.startswith('--sysroot=')],*inc,'-O2','-std=gnu11',str(source/(stem+'.c')),'-o',str(out/('mali_texture_probe' if stem=='probe' else 'mali_burst_probe')),'-lSDL2','-lmali','-lm'],check=True)
