from pathlib import Path
import subprocess,shlex
root=Path(__file__).resolve().parents[3];out=Path('/tmp/rr-atlas');out.mkdir(parents=True,exist_ok=True);build=root/'raverace/build/playable-knulli';(out/'capture.inc').write_text((Path(__file__).parent/'capture.inc').read_text())
s=(root/'engine/quad_gles2.c').read_text();at=s.index('static bool check_gl(');s=s[:at]+'#include "capture.inc"\n'+s[at:];s=s.replace('const draw_command *c = &commands[i];\n        int sampled=SP_SAMPLE;','const draw_command *c = &commands[i];\n        ac_draw(c);\n        int sampled=SP_SAMPLE;');s=s.replace('SP_FULL_FRAME_BEGIN();','SP_FULL_FRAME_BEGIN();\n    memcpy(ac_bg,bg,sizeof ac_bg);',1);s=s.replace('bool ok = flush(stats, END) && (!frame_upload || submit_commands(stats)) && check_gl("end frame");','bool ok = flush(stats, END) && (!frame_upload || submit_commands(stats)) && check_gl("end frame");\n    ac_end();');(out/'quad_capture.c').write_text(s)
s=(root/'engine/tex_bake.c').read_text()+'''
int tex_atlas_capture_key(GLuint t,uint32_t key[14]){for(int i=0;i<TEX_CACHE_SIZE;i++){const TexCacheEntry *e=&tex_cache[i];if(!e->occupied||e->gl_texture!=t)continue;key[1]=e->alloc_w;key[2]=e->alloc_h;key[3]=e->min_u;key[4]=e->min_v;key[5]=e->range_u;key[6]=e->range_v;key[7]=e->texbank;key[8]=e->pal_group;key[9]=e->pal_gen;key[10]=e->cmode;key[11]=e->cap;key[12]=e->used_w;key[13]=e->used_h;return 1;}return 0;}
''';(out/'tex_capture.c').write_text(s)
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();defs=shlex.split(flags.split('C_DEFINES = ',1)[1].split('\n')[0]);includes=shlex.split(flags.split('C_INCLUDES = ',1)[1].split('\n')[0]);opts=shlex.split(flags.split('C_FLAGS = ',1)[1].split('\n')[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0];sysroot=next(x for x in link if x.startswith('--sysroot='))
for source,name,extra in [('quad_capture.c','quad_capture.o',['-DRR_GEOMETRY_INDEX_ORDER','-DRR_ORDERED_SUBMISSION']),('tex_capture.c','tex_capture.o',['-DRR_TEXTURE_TILE_RUNS'])]:subprocess.run([cc,sysroot,*defs,*includes,*opts,*extra,'-I'+str(out),'-c',str(out/source),'-o',str(out/name)],check=True)
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/engine/quad_gles2.c.o'):arg=str(out/'quad_capture.o')
 if arg.endswith('/engine/tex_bake.c.o'):arg=str(out/'tex_capture.o')
 if arg=='rr_gles2':arg=str(out/'capture')
 cmd.append(arg)
subprocess.run(cmd,cwd=build,check=True)
