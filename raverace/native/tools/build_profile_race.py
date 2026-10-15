#!/usr/bin/env python3
"""Build an isolated instrumented v3 source; diagnostic omissions never acceptance."""
import argparse,pathlib
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];s=(root/'native/render/race_v3.c').read_text()
s=s.replace('#include "../../src/rr_video_layers.c"','#include "'+str(root/'src/rr_video_layers.c')+'"')
s=s.replace('static double last_end3;','static double last_end3;\nstatic int profile_mode3;')
s=s.replace('unsigned accounted=0,vertices=0;','double world_cpu_start=clock3(CLOCK_THREAD_CPUTIME_ID);if(profile_mode3==1)glEnable(GL_RASTERIZER_DISCARD);\n unsigned accounted=0,vertices=0;')
s=s.replace('glDrawArraysInstanced(GL_TRIANGLES,0,near?12:6,g->count);','if(profile_mode3!=2)glDrawArraysInstanced(GL_TRIANGLES,0,near?12:6,g->count);')
s=s.replace('CHECK3(accounted==h->visible);double world_submit=', 'CHECK3(accounted==h->visible);if(profile_mode3==1)glDisable(GL_RASTERIZER_DISCARD);double world_cpu=clock3(CLOCK_THREAD_CPUTIME_ID)-world_cpu_start;double world_submit=')
s=s.replace('if(stage_sync3)glFinish();double world_done=', 'double sync_cpu_start=clock3(CLOCK_THREAD_CPUTIME_ID);if(stage_sync3)glFinish();double sync_cpu=clock3(CLOCK_THREAD_CPUTIME_ID)-sync_cpu_start;double world_done=')
s=s.replace('CHECK3(rr_video_layers_compose_gpu(&hud));double hud_submit=', 'double hud_cpu_start=clock3(CLOCK_THREAD_CPUTIME_ID);CHECK3(rr_video_layers_compose_gpu(&hud));double hud_cpu=clock3(CLOCK_THREAD_CPUTIME_ID)-hud_cpu_start;double hud_submit=')
s=s.replace('glFinish();double end=', 'double finish_cpu_start=clock3(CLOCK_THREAD_CPUTIME_ID);glFinish();double finish_cpu=clock3(CLOCK_THREAD_CPUTIME_ID)-finish_cpu_start;double end=')
s=s.replace('if(capture)save3(out,h->tick,"composed");','printf("PROFILE tick=%u mode=%d world_cpu_ms=%.6f world_sync_cpu_ms=%.6f hud_cpu_ms=%.6f finish_cpu_ms=%.6f\\n",h->tick,profile_mode3,world_cpu*1000,sync_cpu*1000,hud_cpu*1000,finish_cpu*1000);\n if(capture)save3(out,h->tick,"composed");')
s=s.replace('CHECK3(argc==7);','profile_mode3=getenv("ENG_NATIVE_PROFILE_MODE")?atoi(getenv("ENG_NATIVE_PROFILE_MODE")):0;CHECK3(profile_mode3>=0&&profile_mode3<=2);CHECK3(argc==7);')
a.out.write_text(s)
