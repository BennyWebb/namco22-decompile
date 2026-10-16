"""P1b-only source transformations: bounded output, warmup and gradual release."""
from pathlib import Path
def apply(name,s,root,target):
 if name=='live.c':
  s=s.replace('static bool p1_legacy_ready=true;','static bool p1_legacy_ready=true;static unsigned p1b_warmed,p1b_stable;static GLuint p1b_fbo,p1b_color;static bool p1b_warming;\nextern void rr_video_p1b_output(unsigned);extern int tex_p1b_release_step(int);extern void rr_p1b_packet(unsigned,const void*,size_t);')

  s=s.replace('live_compile();s->native_compile_ms=live_compile_ms;s->native_tag=1;', 'live_compile();s->native_compile_ms=live_compile_ms;rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);s->native_tag=1;')
  s=s.replace('s->native_tag=0;s->native_walk_ms', 's->p1b_warm=0;s->native_tag=0;s->native_walk_ms')
  s=s.replace('if(p1_legacy_ready&&!rr_p1_legacy_pending()){eng_gles2_close();p1_legacy_ready=false;p1_releases++;}', 'if(p1_legacy_ready&&++p1b_stable>=16&&!rr_p1_legacy_pending()&&tex_p1b_release_step(8)){eng_gles2_close();p1_legacy_ready=false;p1_releases++;fprintf(stderr,"[P1B-RELEASE] completed_tick=%u stable_frames=%u\\n",s->frame,p1b_stable);}')
  import re
  s=re.sub(r'static void save3\(.*?\nstatic uint16_t', 'extern void rr_p1b_pixel(unsigned,const char*,const void*,size_t);\nstatic void save3(const char*out,unsigned tick,const char*suffix){(void)out;glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels3);rr_p1b_pixel(tick,suffix,pixels3,640*480*4); }\nstatic uint16_t',s,flags=re.S)
  s=s.replace('bool rr_p1_draw(const rr_presentation *s,eng_gles2_stats *stats){', 'bool rr_p1_draw(const rr_presentation *s,eng_gles2_stats *stats){if(s->frame==p1b_test_tick&&p1b_test_ms){struct timespec delay={p1b_test_ms/1000,(p1b_test_ms%1000)*1000000};nanosleep(&delay,NULL);fprintf(stderr,"[P1B-TEST-DELAY] tick=%u ms=%u\\n",s->frame,p1b_test_ms);}')
  # Declare the warm state before draw3, whose shader selection uses it.
  s=s.replace('static bool p1_legacy_ready=true;static unsigned p1b_warmed,p1b_stable;static GLuint p1b_fbo,p1b_color;static bool p1b_warming;', 'static bool p1_legacy_ready=true;static unsigned p1b_warmed,p1b_stable;static GLuint p1b_fbo,p1b_color;')
  s=s.replace('static GLuint p1b_fbo,p1b_color;', '')
  s=s.replace('static void draw3(', 'static bool p1b_warming;static GLuint p1b_fbo,p1b_color;static Slot3 p1b_warm_slot;static unsigned p1b_test_tick,p1b_test_ms;\nstatic void draw3(',1)
  needle='glDrawArraysInstanced(GL_TRIANGLES,0,near?12:6,g->count);'
  s=s.replace(needle,needle+'if(p1b_warming){glUseProgram(programs3[near^1]);glDrawArraysInstanced(GL_TRIANGLES,0,near?6:12,g->count);}')
  # Allocate the off-screen final-composition target in startup GL ownership.
  s=s.replace('live_loaded=true;fprintf(stderr,', 'p1b_test_tick=getenv("ENG_P1B_TEST_PRESENT_TICK")?atoi(getenv("ENG_P1B_TEST_PRESENT_TICK")):0;p1b_test_ms=getenv("ENG_P1B_TEST_PRESENT_MS")?atoi(getenv("ENG_P1B_TEST_PRESENT_MS")):0;const char*wp=getenv("ENG_P1B_WARM_PACKET");CHECK3(wp);FILE*wf=fopen(wp,"rb");CHECK3(wf);p1b_warm_slot.size=fread(p1b_warm_slot.bytes,1,RAW3,wf);CHECK3(p1b_warm_slot.size>=sizeof(Header3)&&!fclose(wf));p1b_warm_slot.tick=((Header3*)p1b_warm_slot.bytes)->tick;glGenTextures(1,&p1b_color);glBindTexture(GL_TEXTURE_2D,p1b_color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glGenFramebuffers(1,&p1b_fbo);glBindFramebuffer(GL_FRAMEBUFFER,p1b_fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,p1b_color,0);CHECK3(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glBindFramebuffer(GL_FRAMEBUFFER,0);live_loaded=true;fprintf(stderr,')
  s+='''\nvoid rr_p1b_warm(const rr_presentation*s){if(!s||s->frame+60<live_from||s->frame>=live_from||p1b_warmed>=3)return;unsigned i=p1b_warmed;instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;static float zeros[4096*4];glBindTexture(GL_TEXTURE_2D,instances3);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,MAXI3,GL_RGBA,GL_FLOAT,zeros);glBindTexture(GL_TEXTURE_2D,dynamic3);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4096,1,GL_RGBA,GL_FLOAT,zeros);glBindBuffer(GL_ARRAY_BUFFER,attributes3);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof attribute_data3,attribute_data3);p1b_warming=true;rr_video_p1b_output(p1b_fbo);double t=clock3(CLOCK_MONOTONIC);draw3(&p1b_warm_slot,-1,"-",t,0);glFinish();rr_video_p1b_output(0);rr_video_layers_begin();p1b_warming=false;p1b_warmed++;fprintf(stderr,"[P1B-WARM] tick=%u resource_copy=%u complete_ms=%.6f offscreen=1 shader_variants=2\\n",s->frame,i,(clock3(CLOCK_MONOTONIC)-t)*1000);}\n'''
  s=s.replace('p1_ring_wait*1000);', 'p1_ring_wait*1000);fprintf(stderr,"[P1B-WARM-SUMMARY] completed=%u expected=3\\n",p1b_warmed);')
 if name=='gles.c':
  s=s.replace('}else if(prepared){\n        if(!rr_p1_legacy_ensure()', '}else if(prepared){\n        rr_p1b_warm(prepared);\n        if(!rr_p1_legacy_ensure()')
 if name=='presentation.c':
  s=s.replace('static int p1_slots=3;', 'static unsigned p1b_activations,p1b_replaced;static bool p1b_valve,p1b_wall_mode;static double p1b_periods=2,p1b_next;\nstatic int p1_slots=3;')
  s=s.replace('p1_slots=getenv("ENG_P1_QUEUE")?', 'p1b_valve=getenv("ENG_P1B_VALVE")&&atoi(getenv("ENG_P1B_VALVE"));if(getenv("ENG_P1B_VALVE_PERIODS"))p1b_periods=atof(getenv("ENG_P1B_VALVE_PERIODS"));if(p1b_periods<1||p1b_periods>10)return false;\n    p1_slots=getenv("ENG_P1_QUEUE")?')
  s=s.replace('    double p1_block=0;','''    if(p1b_valve&&p1b_wall_mode&&frame>=1800){double t=now();if(p1b_next>t){struct timespec delay={0,(long)((p1b_next-t)*1e9)};nanosleep(&delay,NULL);}p1b_next=now()+1.0/59.7104;}
    double p1_block=0;''')
  old='while(slot<0&&!worker_failed){double b=now();metadata_wait();p1_block+=now()-b;for(int i=0;i<SLOTS;i++)if(state[i]==FREE){slot=i;break;}}'
  new='''double p1b_wait_start=now();bool p1b_reused=false;
        while(slot<0&&!worker_failed){
            if(p1b_valve&&frame>=1800&&(p1b_wall_mode||now()-p1b_wait_start>=p1b_periods/59.7104)){
                if(!p1b_wall_mode){p1b_wall_mode=true;p1b_activations++;p1b_next=now()+1.0/59.7104;}
                int old=newest(PREPARED); /* Never overwrite WRITING/PREPARING/READING; compile every tick. */
                if(old>=0){fprintf(stderr,"[P1B-REPLACE] admission=%u replaced=%u elapsed_ms=%.6f\\n",frame,slots[old].frame,(now()-p1b_wait_start)*1000);discard_ready(old);p1b_replaced++;p1b_reused=true;slot=old;break;}
            }
            double before=now();
            if(p1b_valve&&frame>=1800){struct timespec end;clock_gettime(CLOCK_REALTIME,&end);end.tv_nsec+=1000000;if(end.tv_nsec>=1000000000){end.tv_sec++;end.tv_nsec-=1000000000;}pthread_cond_timedwait(&changed,&mutex,&end);}
            else metadata_wait();
            p1_block+=now()-before;for(int i=0;i<SLOTS;i++)if(state[i]==FREE){slot=i;break;}
        }'''
  assert old in s;s=s.replace(old,new)
  s=s.replace('        if (slot >= 0) {\n            if (state[slot] == READY)', '        if (slot >= 0) {\n            if(state[slot]==FREE&&!p1b_reused)p1b_wall_mode=false;\n            if (state[slot] == READY)')
  s=s.replace('    fprintf(stderr, "[THREAD-PERIOD]', '    fprintf(stderr,"[P1B-VALVE] enabled=%d periods=%.3f activations=%u replaced=%u\\n",p1b_valve,p1b_periods,p1b_activations,p1b_replaced);\n    fprintf(stderr, "[THREAD-PERIOD]')
 return s

def texture(s):
 return s+'''\n/* P1b native-only cache retirement. Legacy lookup/recreation algorithms unchanged. */
int tex_p1b_release_step(int limit){
 static int cursor;int n=0;GLuint ids[32];if(limit>32)limit=32;
 while(cursor<TEX_CACHE_SIZE&&n<limit){TexCacheEntry*e=&tex_cache[cursor++];if(e->occupied){ids[n++]=e->gl_texture;tex_cache_bytes-=e->bytes;tex_cache_total--;e->occupied=0;e->alloc_w=e->alloc_h=0;}}
 if(n)glDeleteTextures(n,ids);
 if(cursor==TEX_CACHE_SIZE){cursor=0;return 1;}return 0;
}\n'''
