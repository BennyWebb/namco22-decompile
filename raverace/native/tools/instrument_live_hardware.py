#!/usr/bin/env python3
"""Private bounded telemetry only; no gameplay, renderer or pacing changes."""
import pathlib,argparse,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('prototype',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];out=a.out.resolve();assert out.is_relative_to(root/'raverace/native/generated');out.mkdir(parents=True,exist_ok=True);build=a.build.resolve();live=a.prototype.resolve()
s=(live/'gles.c').read_text();mark='static bool failed, unpaced';idx=s.index(mark);s=s[:idx]+'''typedef struct {unsigned tick;double sim,scene,draw,swap,present,wait;unsigned queued,underruns,overruns,resets;} LiveTiming;
static LiveTiming live_timings[8192];static unsigned live_timing_count;
static void live_timing_flush(void){const char*p=getenv("ENG_LIVE_TELEMETRY");if(!p)return;static bool flushed;if(flushed)return;flushed=true;FILE*f=fopen(p,"wx");if(!f){perror(p);exit(2);}fprintf(f,"tick,simulation_ms,scene_ms,draw_ms,swap_ms,present_seconds,host_wait_ms,audio_queued,audio_underruns,audio_overruns,audio_resets\\n");for(unsigned i=0;i<live_timing_count;i++){LiveTiming*r=live_timings+i;fprintf(f,"%u,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u,%u,%u\\n",r->tick,r->sim*1000,r->scene*1000,r->draw*1000,r->swap*1000,r->present,r->wait*1000,r->queued,r->underruns,r->overruns,r->resets);}if(fclose(f))exit(2);}
''' +s[idx:]
mark='    audit_row(frame,true,(unsigned)count,&stats);';assert s.count(mark)==1;s=s.replace(mark,'''    if(getenv("ENG_LIVE_TELEMETRY")){if(live_timing_count>=8192){fprintf(stderr,"telemetry overflow\\n");exit(2);}rr_audio_output_stats audio;rr_audio_output_status(&audio);live_timings[live_timing_count++]=(LiveTiming){frame,simulation_elapsed,scene_elapsed,draw_elapsed,swap_elapsed,last_presentation,host_wait_elapsed,audio.queued,audio.underruns,audio.overruns,audio.resets};}
'''+mark);s=s.replace('    rr_native_close();','    live_timing_flush();\n    rr_native_close();');(out/'gles.c').write_text(s)
s=(root/'raverace/src/rr_sim_profile.c').read_text();needle='    double *values=malloc';idx=s.index(needle);s=s[:idx]+'''    const char *path=getenv("ENG_LIVE_SIM_TELEMETRY");
    if(path){FILE*f=fopen(path,"wx");if(!f){perror(path);exit(2);}fprintf(f,"tick,wall_ms,cpu_ms");for(int k=0;k<SIM_COUNT;k++)fprintf(f,",%s_ms",names[k]);fputc('\\n',f);for(unsigned i=0;i<count;i++){fprintf(f,"%u,%.9f,%.9f",from+i,records[i].wall*1000,records[i].cpu*1000);for(int k=0;k<SIM_COUNT;k++)fprintf(f,",%.9f",records[i].time[k]*1000);fputc('\\n',f);}if(fclose(f))exit(2);}
'''+s[idx:];(out/'sim.c').write_text(s)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0]
for name in ['gles','sim']:subprocess.run([cc,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(root/'raverace/native/live'),'-I'+str(root/'raverace/src'),'-c',str(out/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
replacements={'rr_main.c.o':live/'main.o','rr_gl.c.o':live/'gl.o','rr_gles2.c.o':out/'gles.o','rr_video_layers.c.o':live/'layers.o','rr_sim_profile.c.o':out/'sim.o'};cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file=') or arg.endswith('/engine/geo_hw.c.o') or arg.endswith('/engine/slave_list.c.o'):continue
 if pathlib.Path(arg).name in replacements:arg=str(replacements[pathlib.Path(arg).name])
 if arg=='rr_gles2':arg=str(out/'rr_native_live_telemetry')
 cmd.append(arg)
subprocess.run(cmd+[str(live/'live.o'),'-lz'],cwd=build,check=True)
