"""Relink private copies; production build/source are not edited."""
from pathlib import Path
import subprocess, shlex, sys
root=Path(__file__).resolve().parents[3]
source=Path(__file__).resolve().parent
out=Path(sys.argv[2] if len(sys.argv)>2 else '/tmp/rr-mali-array');out.mkdir(parents=True,exist_ok=True)
build=Path('/tmp/rr-world-array-arm-build')
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(name):return shlex.split(flags.split(name+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0]
base=[cc,next(x for x in link if x.startswith('--sysroot=')),*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(source)]
s=(root/'engine/tex_bake.c').read_text().replace('#include "gles_texture_trace.inc"','#define MT_WRAP\n#include "trace.h"\n#include "gles_texture_trace.inc"').replace('    glBindTexture(target, texture);','    mt_bind(target, texture);')
a=Path(sys.argv[1]).read_text() if len(sys.argv)>1 else (root/'engine/gles_world_array_cache.inc').read_text()
a=a.replace('    world_reap(0);','    double reap_begin=gles_seconds();world_reap(0);mt_scope("reap_scope",reap_begin);\n    double search_begin=gles_seconds();')
a=a.replace('    world_array_page *p=world_pages+index;int slot=', '    mt_scope("search_scope",search_begin);\n    world_array_page *p=world_pages+index;int slot=')
a=a.replace('GLenum status=glClientWaitSync','mt_sync_object(p->texture);GLenum status=glClientWaitSync')
a=a.replace('p->mask&=~','mt_invalidate(p->texture,e->a_x,p->mask);p->mask&=~')
a=a.replace('p->retired=glFenceSync','mt_sync_object(p->texture);p->retired=glFenceSync')
a=a.replace('world_array_page *p=world_pages+i;if(p->retired)', 'world_array_page *p=world_pages+i;mt_sync_object(p->texture);if(p->retired)')
(out/'gles_world_array_cache.inc').write_text(a)
(out/'tex_trace.c').write_text(s)
s=(root/'engine/quad_gles2.c').read_text()
s='#include "trace.h"\n'+s
# Private array submission include shadows the original beside copied source.
a=(root/'engine/gles_world_array_submit.inc').read_text().replace('double call=seconds();glDrawArrays','for(int ri=i;ri<end;ri++)mt_reference(world_order[ri].texture,world_order[ri].layer);\n        double call=seconds();glDrawArrays')
(out/'gles_world_array_submit.inc').write_text(a)
s=s.replace('if(!AUDIT_NO_DRAW) glDrawArrays(GL_TRIANGLES, c->first, c->count);','if(!AUDIT_NO_DRAW) {mt_reference(c->texture,0);glDrawArrays(GL_TRIANGLES, c->first, c->count);}')
(out/'quad_trace.c').write_text(s)
for stem,extra in [('tex_trace',['-DRR_GLES_WORLD_ATLAS','-DRR_TEXTURE_TILE_RUNS']),('quad_trace',['-DRR_GLES_WORLD_ATLAS','-DRR_ORDERED_SUBMISSION','-DRR_GEOMETRY_INDEX_ORDER']),('trace',[])]:
 src=source/'trace.c' if stem=='trace' else out/(stem+'.c')
 subprocess.run([*base,*extra,'-c',str(src),'-o',str(out/(stem+'.o'))],check=True)
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/engine/quad_gles2.c.o'):arg=str(out/'quad_trace.o')
 if arg.endswith('/engine/tex_bake.c.o'):arg=str(out/'tex_trace.o')
 if arg=='rr_gles2':arg=str(out/'rr_mali_trace')
 cmd.append(arg)
cmd.append(str(out/'trace.o'))
subprocess.run(cmd,cwd=build,check=True)
