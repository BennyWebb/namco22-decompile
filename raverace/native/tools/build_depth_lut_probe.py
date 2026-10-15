#!/usr/bin/env python3
"""Bounded native M3 depth/LUT/logging variants. No UBO path compiled."""
import argparse,pathlib,subprocess,sys,json
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);p.add_argument('--linear',action='store_true');a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[2]
subprocess.run([sys.executable,str(root/'native/tools/build_hud_probe.py'),str(a.out)],check=True)
s=(a.out/'probe.c').read_text();layer=(a.out/'layers.c').read_text();hud=(a.out/'rr_gpu_hud.inc').read_text();geo=(a.out/'rr_gpu_hud_geometry.inc').read_text()
s='#define _GNU_SOURCE 1\n'+s
s=s.replace('static int hud_control3;', '#include "'+str(root/'native/render/depth_lut_support.inc')+'"\nstatic int hud_control3;',1)
s=s.replace('hud_control3=getenv(', 'native_front3=getenv("ENG_NATIVE_FRONT")&&!strcmp(getenv("ENG_NATIVE_FRONT"),"1");native_fold3=getenv("ENG_NATIVE_FOLD")&&!strcmp(getenv("ENG_NATIVE_FOLD"),"1");native_ram3=getenv("ENG_NATIVE_LOG")&&!strcmp(getenv("ENG_NATIVE_LOG"),"ram");hud_control3=getenv(',1)
s=s.replace('hud_control3<=2','hud_control3==0')
s=s.replace('double load_start=clock3(CLOCK_MONOTONIC);','printf("NATIVE_OPTIONS front=%d folded=%d log=%s cull=1 trace_cap=%zu\\n",native_front3,native_fold3,native_ram3?"ram":"sync",sizeof trace3);\n double load_start=clock3(CLOCK_MONOTONIC);')
s=s.replace('shadow_enabled=%u\\n",h->tick','shadow_enabled=%u glyph_vertices=%d\\n",h->tick').replace('hud_pen_cpu3,hs[3]);','hud_pen_cpu3,hs[3],glyph_vertex_count);')
# Already-proven transparent-glyph omission, fixed on in all matched variants.
geo='static bool native_blank3[1024];\n'+geo
geo=geo.replace(' bool rebuild=', ''' bool blank3[1024];bool changed3=false;uint8_t transparent3[128];memset(transparent3,255,128);
 for(unsigned k3=0;k3<1024;k3++){const uint8_t*p3=k3<960?frame->characters+k3*128:frame->tilemap+(k3-960)*128;blank3[k3]=!memcmp(p3,transparent3,128);changed3|=blank3[k3]!=native_blank3[k3];}memcpy(native_blank3,blank3,sizeof blank3);
 bool rebuild=changed3||''',1)
geo=geo.replace('code=word&1023;needed_glyphs[code]=1;','code=word&1023;if(blank3[code])continue;needed_glyphs[code]=1;')
cut=geo.index('static const char *glyph_final_no_lut_fs=');stop=geo.index('static void gpu_account',cut);geo=geo[:cut]+geo[stop:]
fold=(root/'native/render/hud_folded.frag').read_text();geo='static const char *native_fold_fs3='+json.dumps(fold)+';\n'+geo
hud=hud.replace('if(gpu_geometry())fs=hud_control3==2?glyph_final_no_lut_fs:glyph_final_fs;','if(gpu_geometry())fs=native_fold3?native_fold_fs3:glyph_final_fs;')
hud=hud.replace('static GLuint gpu_program, raw_tex, palette_tex;','static GLuint gpu_program, raw_tex, palette_tex;\nstatic bool fold_valid3;static uint8_t fold_palette3[1024],fold_lut3[1024],fold_mix3[9];')
start=hud.index(' if(!palette_valid||memcmp(');end=hud.index(' if(rr_video_layers_profile_enabled())',hud.index('\n }',hud.index(' if(!lut_valid||memcmp(',start))+1)
old=hud[start:end]
new=''' if(native_fold3){
 if(!fold_valid3||memcmp(fold_palette3,frame->palette,1024)||memcmp(fold_lut3,frame->lut,1024)||memcmp(fold_mix3,frame->multipliers,9)){
 uint8_t pal3[256][4],rows3[4][256][4];for(unsigned i3=0;i3<256;i3++){for(unsigned c3=0;c3<3;c3++){pal3[i3][c3]=frame->lut[frame->palette[i3][c3]][c3];rows3[0][i3][c3]=frame->lut[i3][c3];for(unsigned r3=0;r3<3;r3++)rows3[r3+1][i3][c3]=frame->lut[(i3*frame->multipliers[r3][c3])>>8][c3];}pal3[i3][3]=255;for(unsigned r3=0;r3<4;r3++)rows3[r3][i3][3]=255;}
 glBindTexture(GL_TEXTURE_2D,palette_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGBA,GL_UNSIGNED_BYTE,pal3);glBindTexture(GL_TEXTURE_2D,lut_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,4,GL_RGBA,GL_UNSIGNED_BYTE,rows3);gpu_account(sizeof pal3+sizeof rows3);
 memcpy(fold_palette3,frame->palette,1024);memcpy(fold_lut3,frame->lut,1024);memcpy(fold_mix3,frame->multipliers,9);fold_valid3=true;
 }}else{
'''+old+' }\n'
hud=hud[:start]+new+hud[end:]
layer=layer.replace('lut_tex=texture(256,1);','lut_tex=texture(256,native_fold3?4:1);')
# Preserve command bytes; only build an alternate visible permutation inside safe groups.
mark='static double last_end3;';code='''static uint16_t front_indices3[MAXQ3];static const Quad3*sort_quads3;
static int compare_depth3(const void*a,const void*b){unsigned x=*(const uint16_t*)a,y=*(const uint16_t*)b;float dx=sort_quads3[x].depth,dy=sort_quads3[y].depth;return dx<dy?-1:dx>dy?1:(x>y)-(x<y);}
'''+mark
s=s.replace(mark,code)
mark=' unsigned culled=0;'
code=''' if(native_front3){memcpy(front_indices3,visible,h->visible*2);sort_quads3=q;for(unsigned gi3=0;gi3<h->groups;gi3++){const Group3*g3=groups+gi3;if(g3->mode)continue;uint16_t*ids3=front_indices3+g3->first;qsort(ids3,g3->count,2,compare_depth3);for(unsigned j3=1;j3<g3->count;j3++){int a3=(int)lroundf((q[ids3[j3-1]].depth*.5f+.5f)*65535.f),b3=(int)lroundf((q[ids3[j3]].depth*.5f+.5f)*65535.f);CHECK3(b3-a3>8);}}visible=front_indices3;}
'''+mark
s=s.replace(mark,code)
if a.linear:
 old=s[s.index(' if(native_front3){'):s.index(' unsigned culled=0;',s.index(' if(native_front3){'))]
 new=''' if(native_front3){memcpy(front_indices3,visible,h->visible*2);for(unsigned gi3=0;gi3<h->groups;gi3++){const Group3*g3=groups+gi3;if(g3->mode)continue;CHECK3(g3->first+g3->count<=h->visible);for(unsigned j3=0;j3<g3->count;j3++){unsigned original3=visible[g3->first+j3];CHECK3(original3<h->n);if(j3){unsigned before3=visible[g3->first+j3-1];CHECK3(q[before3].depth-q[original3].depth>.0003f);}front_indices3[g3->first+g3->count-1-j3]=original3;}}visible=front_indices3;}
'''
 s=s.replace(old,new)
 begin=s.index('static uint16_t front_indices3[MAXQ3];static const Quad3*sort_quads3;');end=s.index('static double last_end3;',begin);s=s[:begin]+'static uint16_t front_indices3[MAXQ3];\n'+s[end:]
 s=s.replace('DEPTH bits=%d front_guard_bins=8','DEPTH bits=%d front_guard_bins=8 algorithm=linear')
s=s.replace('load3(argv[1],argv[2]);decode3', 'load3(argv[1],argv[2]);glBindRenderbuffer(GL_RENDERBUFFER,depth);GLint depth_bits3;glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_DEPTH_SIZE,&depth_bits3);CHECK3(depth_bits3>=16);printf("DEPTH bits=%d front_guard_bins=8\\n",depth_bits3);decode3')
# End-to-next-start attribution. Print GAP records only after the measured lap.
s=s.replace('CHECK3(glGetError()==GL_NO_ERROR);\n printf(', 'end_cpu3=clock3(CLOCK_THREAD_CPUTIME_ID);getrusage(RUSAGE_THREAD,&end_usage3);double gc3=clock3(CLOCK_MONOTONIC),gcc3=clock3(CLOCK_THREAD_CPUTIME_ID);CHECK3(glGetError()==GL_NO_ERROR);check_wall3=clock3(CLOCK_MONOTONIC)-gc3;check_cpu3=clock3(CLOCK_THREAD_CPUTIME_ID)-gcc3;trace_wall3=trace_cpu3=0;\n printf(')
mark='double start=clock3(CLOCK_MONOTONIC);Slot3*s=slots3+ordinal%SLOTS3;'
new='''double start=clock3(CLOCK_MONOTONIC);if(ordinal){Gap3*g3=gaps3+ordinal-1;g3->gap=start-last_end3;g3->cpu=clock3(CLOCK_THREAD_CPUTIME_ID)-end_cpu3;g3->residual=g3->gap-g3->check-g3->log-g3->release;struct rusage next3;getrusage(RUSAGE_THREAD,&next3);g3->voluntary=next3.ru_nvcsw-end_usage3.ru_nvcsw;g3->involuntary=next3.ru_nivcsw-end_usage3.ru_nivcsw;}Slot3*s=slots3+ordinal%SLOTS3;'''
assert mark in s;s=s.replace(mark,new)
mark='draw3(s,pass,argv[6],start,waited);pthread_mutex_lock(&lock3);s->state=0;pthread_cond_broadcast(&free3);pthread_mutex_unlock(&lock3);'
new='''draw3(s,pass,argv[6],start,waited);Gap3*g3=gaps3+ordinal;g3->tick=tick;g3->check=check_wall3;g3->checkcpu=check_cpu3;g3->log=trace_wall3;g3->logcpu=trace_cpu3;double rel3=clock3(CLOCK_MONOTONIC),relcpu3=clock3(CLOCK_THREAD_CPUTIME_ID);pthread_mutex_lock(&lock3);s->state=0;pthread_cond_broadcast(&free3);pthread_mutex_unlock(&lock3);g3->release=clock3(CLOCK_MONOTONIC)-rel3;g3->releasecpu=clock3(CLOCK_THREAD_CPUTIME_ID)-relcpu3;gaps_count3=ordinal+1;'''
assert mark in s;s=s.replace(mark,new)
mark='struct rusage usage;getrusage(RUSAGE_SELF,&usage);'
new='''for(unsigned i3=0;i3+1<gaps_count3;i3++){Gap3*g3=gaps3+i3;printf("GAP tick=%u next_tick=%u gap_ms=%.6f cpu_ms=%.6f check_ms=%.6f check_cpu_ms=%.6f log_ms=%.6f log_cpu_ms=%.6f release_ms=%.6f release_cpu_ms=%.6f residual_ms=%.6f voluntary=%ld involuntary=%ld\\n",g3->tick,g3->tick+1,g3->gap*1000,g3->cpu*1000,g3->check*1000,g3->checkcpu*1000,g3->log*1000,g3->logcpu*1000,g3->release*1000,g3->releasecpu*1000,g3->residual*1000,g3->voluntary,g3->involuntary);}
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);'''
assert mark in s;s=s.replace(mark,new).replace('fflush(stdout);rr_video_layers_close();','native_flush3();rr_video_layers_close();')
for name,data in [('probe.c',s),('layers.c',layer),('rr_gpu_hud.inc',hud),('rr_gpu_hud_geometry.inc',geo)]: (a.out/name).write_text(data)
