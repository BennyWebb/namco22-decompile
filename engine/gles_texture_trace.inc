/* Correctness-only ordered event digest, no ROM payload or per-event logging. */
#ifdef RR_GLES_SUBMISSION_PROFILE
static int tt_enabled;
static uint64_t tt_hash=UINT64_C(14695981039346656037),tt_count;
__attribute__((constructor)) static void tt_init(void) {tt_enabled=getenv("GLES_TEXTURE_TRACE")!=NULL;}
static void tt_event(uint32_t tag,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
 if(!tt_enabled)return;
 uint32_t words[6]={tag,g_eng_frame,a,b,c,d};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<4;j++){tt_hash^=(words[i]>>(8*j))&255;tt_hash*=UINT64_C(1099511628211);}
 tt_count++;
}
__attribute__((destructor)) static void tt_report(void) {
 if(tt_enabled)fprintf(stderr,"[TEXTURE-EVENTS] count=%llu fnv1a64=%016llx scope=ordered_requests_hits_misses_binds_deletes_uploads\n",(unsigned long long)tt_count,(unsigned long long)tt_hash);
}
#else
#define tt_event(tag,a,b,c,d)
#endif
