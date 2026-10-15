#!/usr/bin/env python3
"""Private P1 relink; original sources, build objects and production stay intact."""
import argparse,pathlib,subprocess,shlex,json,hashlib
import instrument
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('target',choices=['host','arm']);p.add_argument('--o1',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];build=a.build.resolve();src=root/'raverace/native/p1';out=root/'raverace/native/generated/p1'/a.target;out.mkdir(parents=True,exist_ok=True)
def put(n,s):(out/n).write_text(s)
def replace(s,old,new):assert old in s,old[:120];return s.replace(old,new)
h=(root/'raverace/include/rr_presentation.h').read_text();h=replace(h,'    geo_quad *quads;','    void *native_payload; unsigned native_tag;\n    double native_walk_ms,native_compile_ms;\n    geo_quad *quads;');put('rr_presentation.h',instrument.header(h))
# Keep the selected validated M3 shader/backend and geometry, replacing ownership only.
base=root/'raverace/native/generated/milestone4a'/('coverage-'+a.target)
s=(base/'live.c').read_text();s=s[:s.index('#include "'+str(root/'raverace/native/live/compiler.inc')+'"')]
s=replace(s,'static uint32_t pw(int i){return g_rr.poly[i&0x7fff];}','static const rr_presentation *p1_source;\nstatic uint32_t pw(int i){return p1_source->poly[i&0x7fff];}')
s='#include "p1_api.h"\n#include <setjmp.h>\nstatic void live_fail(const char*,int,int);\n'+s
s=s.replace('if(count>=32768)abort();','if(count>=32768)live_fail("geometry capture capacity",-1,-1);')
# GL owns all resources. Three copies plus fences ensure no in-flight overwrite.
s=replace(s,'static GLuint pages3[16]', 'static GLuint p1_instances[3],p1_dynamic[3],p1_attributes[3];\nstatic GLsync p1_fences[3];static unsigned p1_gpu_index;\nstatic const rr_video_cpu_frame *p1_hud;\nstatic bool p1_diagnostic;\nstatic GLuint pages3[16]')
s=replace(s,' glFinish();\n}', '''
 p1_instances[0]=instances3;p1_dynamic[0]=dynamic3;p1_attributes[0]=attributes3;
 for(int i=1;i<3;i++){p1_instances[i]=texture3(4,MAXI3,GL_RGBA32F,NULL);p1_dynamic[i]=texture3(4096,1,GL_RGBA32F,NULL);glGenBuffers(1,&p1_attributes[i]);glBindBuffer(GL_ARRAY_BUFFER,p1_attributes[i]);glBufferData(GL_ARRAY_BUFFER,sizeof attribute_data3,NULL,GL_STREAM_DRAW);}
 glFinish();
}''')
s=replace(s,'g_eng_frame=h->tick;CHECK3(rr_gl_video_compose());','CHECK3(rr_gl_video_submit(p1_hud));')
s=replace(s,'glFinish();double end=clock3(CLOCK_MONOTONIC);','if(p1_diagnostic)glFinish();double end=clock3(CLOCK_MONOTONIC);')
# No per-picture stdout in paced runs; existing bounded M3 trace flushes at shutdown.
line=next(l for l in s.splitlines() if 'printf("%s pass=' in l);s=s.replace(line,' if(p1_diagnostic){'+line+'}')
compiler=(root/'raverace/native/live/compiler.inc').read_text();compiler=compiler[:compiler.index('bool rr_native_open')]
compiler=compiler.replace('#include "live_api.h"','')
compiler=compiler.replace('static Slot3 live_slot;static bool live_prepared,live_loaded;','static Slot3 *p1_output;\n#define live_slot (*p1_output)\nstatic bool live_prepared,live_loaded;\nstatic jmp_buf p1_unsupported;static const char *p1_reason;')
start=compiler.index('static void live_fail(');end=compiler.index('static int live_recipe_compare',start);compiler=compiler[:start]+'static struct{unsigned tick;int code,packet;const char *reason;} p1_failures[64];static unsigned p1_failure_count;\nstatic void live_fail(const char *why,int code,int packet){if(p1_failure_count<64){unsigned i=p1_failure_count++;p1_failures[i].tick=live_frame;p1_failures[i].code=code;p1_failures[i].packet=packet;p1_failures[i].reason=why;}p1_reason=why;longjmp(p1_unsupported,1); }\n'+compiler[end:]
compiler=compiler.replace('CHECK3(', 'P1_VALID(')
compiler='#define P1_VALID(x) do{if(!(x))live_fail(\"unsupported command layout\",-1,-1);}while(0)\n'+compiler
compiler=compiler.replace('rr_scene_direct_count()','p1_source->direct_count').replace('rr_scene_direct(i)','p1_source->direct[i]').replace('g_rr.pal','p1_source->pal').replace('g_rr.mixer','p1_source->mixer').replace('g_rr.text','p1_source->text')
opening=(root/'raverace/native/live/compiler.inc').read_text();opening=opening[opening.index('bool rr_native_open'):opening.index('void rr_native_set_frame')].replace('rr_native_open','rr_p1_open').replace('serial=1','serial=0 fifo=1').replace('native_front3=1;', 'p1_diagnostic=getenv(\"ENG_P1_FENCE\")!=NULL;native_front3=1;');s+=compiler+'\n'+opening+'\n'+(src/'native.inc').read_text();put('live.c',s)
# TLS snapshot selector is CPU-only; no palette/backend global mutation on preparer.
gl=(root/'raverace/src/rr_gl.c').read_text();gl=replace(gl,'void rr_gl_presentation_release(void)', 'void rr_gl_p1_select(const rr_presentation *s){render_snapshot=s;}\nvoid rr_gl_presentation_release(void)');put('gl.c',gl)
# Retain the common legacy/native swap, input and original audio path.
gles=(root/'raverace/src/rr_gles2.c').read_text();gles='#include "p1_api.h"\n'+gles;gles=gles.replace('{SDL_GL_CONTEXT_MAJOR_VERSION, 2}','{SDL_GL_CONTEXT_MAJOR_VERSION, 3}')
gles=replace(gles,'SDL_GL_SetSwapInterval((unpaced || independent) ? 0 : 1)','SDL_GL_SetSwapInterval(1)')
gles=replace(gles,'    if (controller_db &&','    if(!rr_p1_open()){failed=true;return false;}\n    if (controller_db &&')
gles=replace(gles,'    if(prepared){\n        rr_capture_prepared_quads', '    if(prepared&&prepared->native_tag){\n        if(!rr_p1_draw(prepared,&stats)){failed=true;return false;}\n    }else if(prepared){\n        if(!rr_p1_legacy_ensure()){failed=true;return false;}\n        rr_capture_prepared_quads')
gles=replace(gles,'    if(prepared ? !rr_gl_video_submit(&prepared->video) : !rr_gl_video_compose())','    if(prepared&&prepared->native_tag ? false : prepared ? !rr_gl_video_submit(&prepared->video) : !rr_gl_video_compose())')
gles=replace(gles,'void rr_gles2_close(void)\n{','void rr_gles2_close(void)\n{') # close only after worker joins
mark='    timing_report("total");';gles=replace(gles,mark,'    rr_p1_close();\n'+mark)
put('gles.c',gles)
# Reuse the existing slot state machine, workers/context transfer and snapshot copier.
s=(root/'raverace/src/rr_presentation.c').read_text();s='#include "p1_api.h"\n'+s
s=s.replace('#define SLOTS 3','static int p1_slots=3;\n#define SLOTS p1_slots').replace('state[SLOTS]','state[3]').replace('tail_end[SLOTS]','tail_end[3]')
s=replace(s,'slots[i].quads);','slots[i].quads);free(slots[i].native_payload);')
s=replace(s,'slots[i].frame > slots[best].frame','slots[i].frame < slots[best].frame')
start=s.index('            /* Physical measurements');end=s.index('            break;',start);s=s[:start]+s[end:]
s=s.replace('for(int i=0;i<SLOTS;i++)if(state[i]==READY)discard_ready(i);','/* Strict FIFO: no READY payload is discarded. */')
s=s.replace('bool ok=rr_gl_presentation_into(s);','bool ok=rr_p1_prepare(s);\n        if(ok&&!s->native_tag)ok=rr_gl_presentation_into(s);\n        else rr_gl_p1_select(s);')
start=s.index('        for (int i = 0; i < SLOTS; i++)\n            if (state[i] == (');end=s.index('        pthread_mutex_unlock(&mutex);',start);s=s[:start]+s[end:]
s=replace(s,'    bool ok = SDL_GL_MakeCurrent(window, context) == 0;','    bool ok = SDL_GL_MakeCurrent(window, context) == 0;\n    if(ok && SDL_GL_SetSwapInterval(1) && !getenv("ENG_P1_VALIDATION"))ok=false;')
s=replace(s,'    sync_owner=&producer_sync;\n    memset', '    p1_slots=getenv("ENG_P1_QUEUE")?atoi(getenv("ENG_P1_QUEUE")):3;\n    if(p1_slots!=2&&p1_slots!=3)return false;\n    sync_owner=&producer_sync;\n    memset')
start=s.index('    rr_clock_begin(&clock_state, now());');end=s.index('    /* Every emulated video boundary',start);s=s[:start]+'    bool selected=true; /* Presentation-paced FIFO; no catch-up/resync/drop. */\n\n'+s[end:]
start=s.index('        if (slot < 0)\n            for (int i = 0; i < SLOTS; i++)');end=s.index('        if (slot >= 0)',start)
s=s[:start]+'''        while(slot<0&&!worker_failed){metadata_wait();for(int i=0;i<SLOTS;i++)if(state[i]==FREE){slot=i;break;}}
'''+s[end:]
s=replace(s,'snapshot->frame = frame; snapshot->walk = walk;', 'snapshot->native_tag=0; snapshot->frame = frame; snapshot->walk = walk;')
# No renderer CPU globals are touched by simulation; old background only used for legacy.
s=replace(s,'if(parallel){quads=snapshot->quads;count=snapshot->quad_count;}','if(parallel){quads=snapshot->quads;count=snapshot->native_tag?0:snapshot->quad_count;}')
s+='\nbool rr_p1_legacy_pending(void){bool pending=false;metadata_lock();for(int i=0;i<SLOTS;i++)if(state[i]==PREPARED&&!slots[i].native_tag)pending=true;pthread_mutex_unlock(&mutex);return pending;}\n';put('presentation.c',instrument.presentation(s))
audio=(root/'raverace/src/rr_audio.c').read_text();audio=audio.replace('static double rs_pos;', 'static double p1_adj=1,p1_adj_min=1,p1_adj_max=1;double rr_p1_audio_ratio(void){return p1_adj;}\nstatic double rs_pos;');audio=audio.replace('    const double step =', '    p1_adj=adj;if(adj<p1_adj_min)p1_adj_min=adj;if(adj>p1_adj_max)p1_adj_max=adj;\n    const double step =');audio=audio.replace('        SDL_CloseAudioDevice(dev);','        fprintf(stderr,\"[P1-AUDIO] resampler_ratio_min=%.9f max=%.9f final=%.9f\\n\",p1_adj_min,p1_adj_max,p1_adj);\n        SDL_CloseAudioDevice(dev);');put('audio.c',audio)
# Selected M3 HUD: per-picture CPU input is the original prepared-video API.
layer=(base/'layers.o');assert layer.exists()
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0];incs=['-I'+str(out),'-I'+str(src),'-I'+str(root/'raverace/src'),'-I'+str(root/'raverace/native/live')]
commands=[]
for n in ['gl','gles','presentation','live','audio']:
 cmd=[cc,'-D_GNU_SOURCE',*get('C_DEFINES'),*incs,*get('C_INCLUDES'),*get('C_FLAGS'),'-c',str(out/(n+'.c')),'-o',str(out/(n+'.o'))];commands.append(cmd);subprocess.run(cmd,check=True)
flags=root/'raverace/native/generated/stage1b'/a.target/('O1' if a.o1 else 'O2');replacements={'rr_audio.c.o':out/'audio.o','rr_gl.c.o':out/'gl.o','rr_gles2.c.o':out/'gles.o','rr_presentation.c.o':out/'presentation.o','rr_video_layers.c.o':layer,'rr_lifted.c.o':flags/'rr_lifted.o','snd_driver.c.o':flags/'snd_driver.o'}
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file=') or arg.endswith('/engine/geo_hw.c.o') or arg.endswith('/engine/slave_list.c.o'):continue
 if pathlib.Path(arg).name in replacements:arg=str(replacements[pathlib.Path(arg).name])
 if arg=='rr_gles2':arg=str(out/'rr_p1')
 cmd.append(arg)
cmd.extend([str(out/'live.o'),'-lz']);commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
(out/'build.json').write_text(json.dumps(dict(commands=commands,generated_flags='O1' if a.o1 else 'O2',executable_sha256=hashlib.sha256((out/'rr_p1').read_bytes()).hexdigest()),indent=2)+'\n')
print(out/'rr_p1')
