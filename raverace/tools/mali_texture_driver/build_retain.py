"""One private candidate: retain signalled empty arrays within original cap.
Original atlas/production objects and executable remain independent fallbacks.
"""
from pathlib import Path
import subprocess,shlex,sys
root=Path(__file__).resolve().parents[3];out=Path(sys.argv[2] if len(sys.argv)>2 else '/tmp/rr-mali-array');out.mkdir(parents=True,exist_ok=True);build=Path(sys.argv[1] if len(sys.argv)>1 else '/tmp/rr-world-array-arm-build')
s=(root/'engine/gles_world_array_cache.inc').read_text()
s=s.replace('static unsigned long long world_array_allocations', 'static unsigned long long world_idle_reclaims,world_blocking_polls;\nstatic unsigned long long world_array_allocations')
s=s.replace('GLenum status=glClientWaitSync', 'if(blocking)world_blocking_polls++;\n        GLenum status=glClientWaitSync')
s=s.replace('static void world_reap(int blocking)\n{','static int world_retain(void){static int initialized,enabled;if(!initialized){initialized=1;enabled=getenv("ENG_WORLD_ARRAY_RETAIN")!=NULL;}return enabled;}\nstatic void world_reap(int blocking)\n{\n    if(blocking && world_retain())for(int i=0;i<WORLD_ARRAY_PAGES;i++){world_array_page *p=world_pages+i;if(p->texture && !p->mask && !p->retired){world_idle_reclaims++;glDeleteTextures(1,&p->texture);world_array_bytes-=(size_t)p->w*p->h*8;memset(p,0,sizeof *p);return;}}')
s=s.replace('world_array_page *p=world_pages+i;if(!p->retired)continue;','world_array_page *p=world_pages+i;\n        if(!p->retired)continue;')
s=s.replace('glDeleteSync(p->retired);glDeleteTextures(1,&p->texture);','glDeleteSync(p->retired);p->retired=NULL;\n        if(!blocking && world_retain())continue;\n        glDeleteTextures(1,&p->texture);')
s=s.replace('fence_waits=%llu metadata_bytes=', 'capacity_reap_calls=%llu metadata_bytes=')
s=s.replace('static void world_cache_report(void)\n{','static void world_cache_report(void)\n{\n    fprintf(stderr,"[WORLD-RETAIN] enabled=%d idle_reclaims=%llu blocking_fence_polls=%llu\\n",world_retain(),world_idle_reclaims,world_blocking_polls);')
(out/'gles_world_array_cache.inc').write_text(s)
(out/'tex_retain.c').write_text((root/'engine/tex_bake.c').read_text())
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(name):return shlex.split(flags.split(name+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0]
subprocess.run([cc,*[x for x in link if x.startswith('--sysroot=')],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-DRR_GLES_WORLD_ATLAS','-DRR_TEXTURE_TILE_RUNS','-c',str(out/'tex_retain.c'),'-o',str(out/'tex_retain.o')],check=True)
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/engine/tex_bake.c.o'):arg=str(out/'tex_retain.o')
 if arg=='rr_gles2':arg=str(out/'rr_mali_retain')
 cmd.append(arg)
subprocess.run(cmd,cwd=build,check=True)
