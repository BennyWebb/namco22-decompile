#!/usr/bin/env python3
"""Private Stage1 CPU audit. No optimisation or new execution architecture."""
import pathlib,argparse,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('prototype',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];build=a.build.resolve();proto=a.prototype.resolve();out=a.out.resolve();assert out.is_relative_to(root/'raverace/native/generated');out.mkdir(parents=True,exist_ok=True)
c=(root/'raverace/native/live/compiler.inc').read_text();pos=c.index('static void live_compile');c=c[:pos]+'''#include "'''+str(root/'raverace/native/audit/stage1_support.inc')+'''"
static void emit(const geo_quad*q,void*u){uint64_t t=s1_detail?s1_counter():0;emit_base(q,u);if(s1_detail)s1_emit+=s1_counter()-t;}
'''+c[pos:]
c=c.replace('double start=clock3(CLOCK_MONOTONIC);unsigned direct=', 'double start=clock3(CLOCK_MONOTONIC);s1_compile_begin();unsigned direct=',1)
replacements=[('  const M3Quad*r=records+i;','  s1_mark(1);const M3Quad*r=records+i;'),('  int u0=4096,u1=0,v0=4096,v1=0;for','  s1_mark(2);int u0=4096,u1=0,v0=4096,v1=0;for'),('  int32_t recipe[]={r->meta','  s1_mark(3);int32_t recipe[]={r->meta'),('  for(int j=0;j<3;j++){','  s1_mark(4);for(int j=0;j<3;j++){'),('  c->reason=live_cull(r);','  s1_mark(5);c->reason=live_cull(r);'),(' for(unsigned i=0;i<direct;i++){',' s1_mark(6);for(unsigned i=0;i<direct;i++){'),(' qsort(live_cmd,n,sizeof *live_cmd,live_painter);for',' s1_mark(7);qsort(live_cmd,n,sizeof *live_cmd,live_painter);s1_mark(8);for'),('qsort(live_cmd,n,sizeof *live_cmd,live_batch);','s1_mark(9);qsort(live_cmd,n,sizeof *live_cmd,live_batch);'),(' Instance3 ins[MAXI3];',' s1_mark(10);Instance3 ins[MAXI3];'),(' qsort(visible,nv,2,live_visible);',' s1_mark(11);qsort(visible,nv,2,live_visible);s1_mark(12);'),(' Header3 h={',' s1_mark(13);Header3 h={')]
for x,y in replacements:assert c.count(x)==1,(x,c.count(x));c=c.replace(x,y,1)
c=c.replace(' const char *dir=getenv("ENG_LIVE_COMMAND_DUMP");',' s1_compile_end(n,ni,nv,ng);s1_row->compile_wall=live_compile_ms/1000;\n const char *dir=getenv("ENG_LIVE_COMMAND_DUMP");',1)
x=c.index('void rr_native_prepare(');y=c.index('bool rr_native_draw(',x);c=c[:x]+'''void rr_native_prepare(bool slave,bool geometry){bool walk=rr_scene_frame(slave);rr_gles2_scene_tick(walk,rr_scene_direct_count());count=0;CHECK3(s1_count<6006);s1_row=&s1_rows[s1_count++];s1_row->tick=live_frame;s1_row->order=!(live_frame&1);s1_snapshot_copy(live_frame,walk);if(s1_row->order)s1_original();s1_row->native_core=sched_getcpu();s1_emit=0;double cpu=s1_cpu(),t=clock3(CLOCK_MONOTONIC);if(geometry){if(walk){eng_list_cfg cfg={ENG_LIST_HEAD_S22,1,NULL,NULL,NULL,NULL};eng_walk_list(pw,&cfg,emit,NULL);}live_walk_ms=(clock3(CLOCK_MONOTONIC)-t)*1000;s1_row->walk_wall=live_walk_ms/1000;s1_row->walk_cpu=s1_cpu()-cpu;s1_row->emit_wall=s1_emit/s1_frequency;live_compile();}s1_row->native_end_core=sched_getcpu();if(!s1_row->order)s1_original();CHECK3(s1_snapshot.quad_count==(int)(count+rr_scene_direct_count()));rr_scene_consume();}
'''+c[y:]
c=c.replace('live_loaded=true;fprintf','live_loaded=true;s1_open();fprintf');c=c.replace('if(!live_loaded)return;fprintf','if(!live_loaded)return;s1_close();fprintf');(out/'compiler.inc').write_text(c)
s=(proto/'live.c').read_text().replace('static void emit(', 'static void emit_base(');s=s.replace(str(root/'raverace/native/live/compiler.inc'),str(out/'compiler.inc'));(out/'live.c').write_text(s)
# Existing geometry-preparation API, only coarse timer probes added.
s=(proto/'gl.c').read_text();s='extern double s1_seconds(void),s1_original_parts[3];\n'+s
start=s.index('static void presentation_geometry(');end=s.index('void rr_gl_presentation(',start);piece=s[start:end];piece=piece.replace('    for(unsigned i=0;i<snapshot->direct_count;i++)','    double audit_start=s1_seconds();\n    for(unsigned i=0;i<snapshot->direct_count;i++)');piece=piece.replace('    if(snapshot->walk) {','    s1_original_parts[0]=s1_seconds()-audit_start;audit_start=s1_seconds();\n    if(snapshot->walk) {');piece=piece.replace('    eng_quad_sort(qbuf,qn,0);','    s1_original_parts[1]=s1_seconds()-audit_start;audit_start=s1_seconds();\n    eng_quad_sort(qbuf,qn,0);\n    s1_original_parts[2]=s1_seconds()-audit_start;');s=s[:start]+piece+s[end:];(out/'gl.c').write_text(s)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0]
for name in ['live','gl']:subprocess.run([cc,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(root/'raverace/src'),'-I'+str(root/'raverace/native/live'),'-I'+str(proto/'m3'),'-c',str(out/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
telemetry=root/'raverace/native/generated/milestone4a-hardware/telemetry-arm-v2'
if 'aarch64' not in cc:telemetry=proto
replace={'rr_main.c.o':proto/'main.o','rr_gl.c.o':out/'gl.o','rr_gles2.c.o':telemetry/'gles.o','rr_video_layers.c.o':proto/'layers.o'}
if (telemetry/'sim.o').exists():replace['rr_sim_profile.c.o']=telemetry/'sim.o'
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file=') or arg.endswith('/engine/geo_hw.c.o') or arg.endswith('/engine/slave_list.c.o'):continue
 if pathlib.Path(arg).name in replace:arg=str(replace[pathlib.Path(arg).name])
 if arg=='rr_gles2':arg=str(out/'rr_stage1')
 cmd.append(arg)
subprocess.run(cmd+[str(out/'live.o'),'-lz'],cwd=build,check=True)
