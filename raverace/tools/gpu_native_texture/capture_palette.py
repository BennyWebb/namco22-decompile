"""Private native relink exports last presented palette; no renderer changes."""
from pathlib import Path
import subprocess,shlex
root=Path(__file__).resolve().parents[3];out=Path('/tmp/rr-native-texture');build=Path('/tmp/rr-playable-native-build')
s=(root/'engine/tex_bake.c').read_text()+r'''
__attribute__((destructor))static void export_palette(void){const char*p=getenv("ENG_NATIVE_PALETTE_CAPTURE");if(!p)return;FILE*f=fopen(p,"wx");if(!f)abort();fwrite(direct_palette,1,sizeof direct_palette,f);fclose(f);}
''';(out/'palette_capture.c').write_text(s)
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(name):return shlex.split(flags.split(name+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text())
subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-DRR_GLES_WORLD_ATLAS','-DRR_TEXTURE_TILE_RUNS','-c',str(out/'palette_capture.c'),'-o',str(out/'palette_capture.o')],check=True)
cmd=[]
for x in link:
 if x.startswith('-Wl,--dependency-file='):continue
 if x.endswith('/engine/tex_bake.c.o'):x=str(out/'palette_capture.o')
 if x=='rr_gles2':x=str(out/'rr_palette_capture')
 cmd.append(x)
subprocess.run(cmd,cwd=build,check=True)
