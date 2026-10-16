/* All compiler scratch is owned by rr-prepare. Source data is a const slot;
 * GL state and GPU resource copies are exclusively owned by rr-present. */
static bool p1_legacy_ready=true;
static unsigned p1_fallbacks,p1_native_count,p1_releases,p1_recreates;
static double p1_ring_wait;
bool rr_p1_prepare(rr_presentation *s){
 s->native_tag=0;s->native_walk_ms=s->native_compile_ms=0;
 if(!live_loaded||s->frame<live_from)return true;
 p1_source=s;live_frame=s->frame;rr_gl_p1_select(s);
 if(!s->native_payload){s->native_payload=calloc(1,sizeof(Slot3));if(!s->native_payload)return false;}
 p1_output=s->native_payload;
 if(setjmp(p1_unsupported)){p1_fallbacks++;return true;}
 if(getenv("ENG_P1_TEST_FALLBACK")&&s->frame==(unsigned)atoi(getenv("ENG_P1_TEST_FALLBACK")))live_fail("diagnostic forced missing mesh",-1,-1);
 count=0;double t=clock3(CLOCK_MONOTONIC);
 if(s->walk){eng_list_cfg cfg={ENG_LIST_HEAD_S22,1,NULL,NULL,NULL,NULL};eng_walk_list(pw,&cfg,emit,NULL);}
 s->native_walk_ms=(clock3(CLOCK_MONOTONIC)-t)*1000;
 live_compile();s->native_compile_ms=live_compile_ms;s->native_tag=1;
 return true;
}
bool rr_p1_draw(const rr_presentation *s,eng_gles2_stats *stats){
 if(p1_legacy_ready&&!rr_p1_legacy_pending()){eng_gles2_close();p1_legacy_ready=false;p1_releases++;}
 unsigned i=p1_gpu_index++%3;
 double start=clock3(CLOCK_MONOTONIC);
 if(p1_fences[i]){GLenum r;do{r=glClientWaitSync(p1_fences[i],GL_SYNC_FLUSH_COMMANDS_BIT,100000000);}while(r==GL_TIMEOUT_EXPIRED);CHECK3(r!=GL_WAIT_FAILED);glDeleteSync(p1_fences[i]);p1_fences[i]=0;}
 p1_ring_wait+=clock3(CLOCK_MONOTONIC)-start;
 instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;
 draw3(s->native_payload,0,getenv("ENG_LIVE_CAPTURE")?getenv("ENG_LIVE_CAPTURE"):"-",clock3(CLOCK_MONOTONIC),0);
 p1_fences[i]=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);CHECK3(p1_fences[i]);
 Header3*h=(Header3*)((Slot3*)s->native_payload)->bytes;memset(stats,0,sizeof *stats);stats->draws=h->groups;stats->triangles=h->visible*2;p1_native_count++;
 return true;
}
bool rr_p1_legacy_ensure(void){if(p1_legacy_ready)return true;if(!eng_gles2_init())return false;p1_legacy_ready=true;p1_recreates++;return true;}
void rr_p1_close(void){if(!live_loaded)return;for(int i=0;i<3;i++)if(p1_fences[i]){glDeleteSync(p1_fences[i]);p1_fences[i]=0;}fprintf(stderr,"[P1] native=%u unsupported_fallback=%u legacy_releases=%u legacy_recreates=%u gpu_ring_wait_ms=%.6f\n",p1_native_count,p1_fallbacks,p1_releases,p1_recreates,p1_ring_wait*1000);for(unsigned i=0;i<p1_failure_count;i++)fprintf(stderr,"[P1-FALLBACK] tick=%u code=%d packet=%d reason=%s\n",p1_failures[i].tick,p1_failures[i].code,p1_failures[i].packet,p1_failures[i].reason);native_flush3();live_loaded=false;}
