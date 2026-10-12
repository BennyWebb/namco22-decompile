/* Opt-in wall-clock attribution; no scheduler or renderer policy changes. */
static bool profile_stages;
static double stage_frame[RR_STAGE_COUNT];
typedef struct {
    unsigned n, first, last;
    double stage[RR_STAGE_COUNT], core, scene_other, build, lookup, uploads;
    double vertex, submit_other, draw, swap, wait, capture;
    unsigned long long quads, triangles, draws, misses, evictions, texture_uploads;
    size_t gpu_end, gpu_peak;
} stage_cohort;
static stage_cohort stage_total[2], stage_window[2];
static double stage_start, stage_end, stage_window_start;

double rr_gles2_stage_begin(void) { return profile_stages ? seconds() : 0; }
void rr_gles2_stage_end(rr_gles2_stage stage, double start)
{ if (profile_stages) stage_frame[stage] += seconds()-start; }
static void stages_report(const char *kind, stage_cohort *cohorts, double start)
{
    unsigned n=cohorts[0].n+cohorts[1].n;
    if (!n) return;
    double elapsed=stage_end-start;
    fprintf(stderr,"[STAGES-RATE] %s timing=%s steps=%u rendered=%u skipped=%u wall_s=%.3f sim_hz=%.2f render_fps=%.2f\n",
        kind,independent?"independent":"coupled",n,cohorts[0].n,cohorts[1].n,elapsed,
        elapsed>0?n/elapsed:0,elapsed>0?cohorts[0].n/elapsed:0);
    for (int k=0;k<2;k++) {
        const stage_cohort *p=&cohorts[k]; if (!p->n) continue;
        double ms=1000.0/p->n;
        fprintf(stderr,"[STAGES] %s timing=%s cohort=%s frames=%u..%u samples=%u avg_ms core68k_other=%.3f dsp=%.3f sound=%.3f audio_mix=%.3f serial=%.3f board_io=%.3f video_latch=%.3f palette=%.3f direct_decode=%.3f list_decode=%.3f sort=%.3f scene_other=%.3f clip_build_state=%.3f texture_lookup_decode=%.3f texture_upload=%.3f vertex_upload=%.3f submission_other=%.3f draw_call=%.3f swap=%.3f host_wait=%.3f capture=%.3f avg_quads=%.1f avg_triangles=%.1f avg_draws=%.1f misses=%llu evictions=%llu texture_uploads=%llu GPU_end=%zu GPU_frame_end_peak=%zu\n",
            kind,independent?"independent":"coupled",k?"skipped":"rendered",p->first,p->last,p->n,
            p->core*ms,p->stage[RR_STAGE_DSP]*ms,p->stage[RR_STAGE_SOUND]*ms,p->stage[RR_STAGE_AUDIO]*ms,
            p->stage[RR_STAGE_SERIAL]*ms,p->stage[RR_STAGE_IO]*ms,p->stage[RR_STAGE_VIDEO]*ms,
            p->stage[RR_STAGE_PALETTE]*ms,p->stage[RR_STAGE_DIRECT]*ms,p->stage[RR_STAGE_LIST]*ms,p->stage[RR_STAGE_SORT]*ms,
            p->scene_other*ms,p->build*ms,p->lookup*ms,p->uploads*ms,p->vertex*ms,p->submit_other*ms,p->draw*ms,
            p->swap*ms,p->wait*ms,p->capture*ms,(double)p->quads/p->n,(double)p->triangles/p->n,(double)p->draws/p->n,
            p->misses,p->evictions,p->texture_uploads,p->gpu_end,p->gpu_peak);
    }
}
static void stages_add(uint32_t frame, bool rendered, unsigned quads, const eng_gles2_stats *s,
                       double scene, double draw_total, double swap, double wait, double capture_time)
{
    if (!profile_stages) return;
    if (frame>=profile_from) {
        if (!stage_start) stage_start=stage_window_start=simulation_start;
        stage_end=seconds();
        for (int group=0;group<2;group++) {
            stage_cohort *p=&(group?stage_window:stage_total)[rendered?0:1];
            if (!p->n) p->first=frame;
            p->n++;p->last=frame;p->quads+=quads;
            double devices=0,host_scene=0;
            for(int i=0;i<RR_STAGE_COUNT;i++) {
                p->stage[i]+=stage_frame[i];
                if(i<=RR_STAGE_IO) devices+=stage_frame[i]; else host_scene+=stage_frame[i];
            }
            p->core+=simulation_elapsed-devices;
            p->scene_other+=scene-host_scene;
            p->build+=draw_total-s->texture_seconds-s->upload_seconds-s->draw_seconds;
            p->lookup+=s->texture_seconds;p->uploads+=s->upload_seconds;
            p->vertex+=s->vertex_upload_seconds;p->draw+=s->draw_call_seconds;
            p->submit_other+=s->draw_seconds-s->vertex_upload_seconds-s->draw_call_seconds;
            p->swap+=swap;p->wait+=wait;p->capture+=capture_time;
            p->triangles+=s->triangles;p->draws+=s->draws;
            p->misses+=s->cache_misses;p->evictions+=s->cache_evictions;p->texture_uploads+=s->texture_uploads;
            p->gpu_end=tex_bake_gles2_resident_bytes();if(p->gpu_end>p->gpu_peak)p->gpu_peak=p->gpu_end;
        }
        if (stage_end-stage_window_start>=5) {
            stages_report("window",stage_window,stage_window_start);
            memset(stage_window,0,sizeof stage_window);stage_window_start=stage_end;
        }
    }
    memset(stage_frame,0,sizeof stage_frame);
}
