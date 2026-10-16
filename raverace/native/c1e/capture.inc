/* Validation only. Bounded transport; rr-present only copies to memory. */
#define C1E_SLOTS 4
#define C1E_BYTES (640*480*24)
typedef struct {unsigned kind,tick;char suffix[16];uint8_t bytes[C1E_BYTES];} C1eJob;
static C1eJob c1e_jobs[C1E_SLOTS];static _Atomic unsigned c1e_head,c1e_tail,c1e_stop,c1e_drops,c1e_errors;static pthread_t c1e_writer;static bool c1e_started;
static void*c1e_drain(void*unused){(void)unused;pthread_setname_np(pthread_self(),"c1e-capture");struct sched_param sp={0};pthread_setschedparam(pthread_self(),SCHED_IDLE,&sp);setpriority(PRIO_PROCESS,0,19);
 for(;;){unsigned t=atomic_load_explicit(&c1e_tail,memory_order_relaxed),h=atomic_load_explicit(&c1e_head,memory_order_acquire);if(t==h){if(atomic_load(&c1e_stop))break;struct timespec d={0,2000000};nanosleep(&d,NULL);continue;}C1eJob*j=c1e_jobs+(t%C1E_SLOTS);const char*dir=getenv(j->kind==1?"ENG_C1_IMAGES":"ENG_C1_PROVENANCE_DIR");
  for(unsigned k=0;k<(j->kind==1?1:3);k++){char path[4096];const char*ext=j->kind==1?j->suffix:k==0?"provenance.u32":k==1?"proof.rgba":"texel.rgba";snprintf(path,sizeof path,j->kind==1?"%s/tick-%u.%s.rgba":"%s/tick-%u.%s",dir,j->tick,ext);size_t offset=j->kind==1?0:k==0?0:k==1?640*480*16:640*480*20;size_t n=j->kind==1||k?640*480*4:640*480*16;FILE*f=__real_fopen(path,"wbx");if(!f){atomic_fetch_add(&c1e_errors,1);continue;}if(fwrite(j->bytes+offset,1,n,f)!=n||fclose(f))atomic_fetch_add(&c1e_errors,1);}
  atomic_store_explicit(&c1e_tail,t+1,memory_order_release);
 }return NULL;
}
static C1eJob*c1e_job(unsigned kind,unsigned tick,const char*suffix){if(!c1e_started){c1e_started=true;if(pthread_create(&c1e_writer,NULL,c1e_drain,NULL))abort();}unsigned h=atomic_load_explicit(&c1e_head,memory_order_relaxed),t=atomic_load_explicit(&c1e_tail,memory_order_acquire);if(h-t==C1E_SLOTS){atomic_fetch_add(&c1e_drops,1);return NULL;}C1eJob*j=c1e_jobs+(h%C1E_SLOTS);j->kind=kind;j->tick=tick;if(suffix)snprintf(j->suffix,sizeof j->suffix,"%s",suffix);return j;}
static void c1e_colour(unsigned tick,const char*suffix,const void*p,size_t n){C1eJob*j=c1e_job(1,tick,suffix);if(j){memcpy(j->bytes,p,n);atomic_fetch_add_explicit(&c1e_head,1,memory_order_release);}}
void rr_c1d_frame(unsigned tick,const void*m,const void*c,const void*r){C1eJob*j=c1e_job(2,tick,NULL);if(j){memcpy(j->bytes,m,640*480*16);memcpy(j->bytes+640*480*16,c,640*480*4);memcpy(j->bytes+640*480*20,r,640*480*4);atomic_fetch_add_explicit(&c1e_head,1,memory_order_release);}}
static void c1e_capture_finish(void){if(c1e_started){atomic_store(&c1e_stop,1);pthread_join(c1e_writer,NULL);}dprintf(summary_fd,"[C1E-CAPTURE] frames=%u drops=%u errors=%u slots=%u bytes=%u fully_drained=1\n",atomic_load(&c1e_head),atomic_load(&c1e_drops),atomic_load(&c1e_errors),C1E_SLOTS,C1E_SLOTS*C1E_BYTES);if(atomic_load(&c1e_errors)||atomic_load(&c1e_drops))_exit(96);}
