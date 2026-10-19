/*
 * rr_main.c -- host for the lifted Rave Racer program.
 *
 * The 68020 program is gen/rr_lifted.c: entry_reset (L_4000) never returns,
 * it runs the game's own main loop. Time advances in rr_tick(), which the
 * lifted code polls on every call and backward branch (RR_POLL). Every
 * RR_POLLS_PER_FRAME polls is one video frame: the vblank devices run, the
 * vblank IRQ is raised, and any pending IRQ above the SR mask is delivered by
 * calling its autovector handler (vector table at VBR = 0).
 *
 * Ghidra's rte p-code is a bare return that restores neither SR nor SP, so an
 * interrupt pushes NO frame: the host saves SR, raises the mask to the
 * interrupt's level, calls the handler, and restores SR afterwards.
 *
 *   rr <rom_dir> [--frames N] [--dump DIR]    headless run
 */
#include "rr_romzip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rr_mem.h"
#include "rr_hw.h"
#include "rr_dsp.h"
#include "rr_video.h"
#include "rr_gl.h"
#include "rr_scene.h"
#include "rr_lift_rt.h"
#include "lift_cpu.h"
#include "rr_game.h"
#include "rr_lifted.h"
#include "rr_sound.h"
#include "rr_input_stream.h"
#include "rr_link.h"

static int32_t test_gas = -1;
static int32_t test_coins[8]; static int n_test_coins;
static struct { int32_t at; int32_t value; } test_steer[8]; static int n_steer;

#define REG_SP   RR_REG_SP
#define REG_SR   RR_REG_SR

static int32_t  polls_per_frame = 83000;   /* 68K instructions per frame (MAME: 4.99M in 60) */
static uint32_t max_frames = 600;
#define frame rr_frame              /* the shared runtime names the frame in its traps */

void rd_budget_out(void);   /* src/rd: unwind a checker probe (budget ran out, or it trapped) */

/* The 68K host runtime -- register file, shadow return stack, traps, interrupt entry, the
 * instruction-trace hook -- is engine/lift_cpu.c, shared with every lifted game. */
#define get_sr rr_get_sr
#define set_sr rr_set_sr

/* Time advances in SLICES: each slice the 68K spends polls_per_frame/SLICES
 * polls and the master DSP its share of a frame (40 MHz C71 = 10 MIPS =
 * ~166,667 instructions per 60 Hz frame); every SLICES slices is a frame. */
#define SLICES 16
#define DSP_STEPS_PER_FRAME 166667L
static uint32_t slice, serial_acc;
/* VTOTAL 525, VBSTART 480 (namcos22.cpp): vblank is the first 45/525 of a frame
 * after the screen update, ~1.37 of our 16 slices. The master answers the
 * vblank INT0 inside that window, and MAME's pdp_begin then sets pdp_frame one
 * AHEAD so the next screen update keeps the list (render_frame_active). */
bool g_rr_in_vblank;
static int vblank_slices;

#include "rr_gles3.h"
#include "rr_presentation.h"
#include "rr_sim_profile.h"
static bool headless_platform_options(int *argc, char **argv) { return rr_gles3_options(argc, argv); }
static bool headless_platform_interactive(void) { return rr_gles3_interactive(); }
static void headless_platform_close(void) { rr_audio_close(); rr_gles3_close(); }
static bool headless_platform_open(const char *dir) { return rr_gl_init(dir) && rr_gles3_open(); }
static bool headless_platform_start(void) {
    rr_memory_profile_range(rr_gles3_profile_from(), max_frames);
    return rr_sim_start();
}
#include "rr_headless.inc"

void rr_tick(void)
{
    rr_sim_stage sim_parent=rr_sim_enter(SIM_SCHEDULER);
    rd_budget_out();
    rr_budget = polls_per_frame / SLICES;
    if (rr_in_irq) { rr_budget = 1000;
        rr_sim_leave(sim_parent);
        return; }      /* finish the handler first */
    double stage_start=rr_gles3_stage_begin();
    rr_sim_enter(SIM_DSP);
    g_rr_in_vblank = vblank_slices > 0;
    if (vblank_slices > 0) {                       /* split the vblank slice at VBEND */
        long vb = DSP_STEPS_PER_FRAME * 45 / 525 - (long)(2 - vblank_slices) * (DSP_STEPS_PER_FRAME / SLICES);
        long full = DSP_STEPS_PER_FRAME / SLICES;
        if (vb > full) vb = full;
        if (vb < 0) vb = 0;
        rr_dsp_run(vb);
        g_rr_in_vblank = false;
        rr_dsp_run(full - vb);
        vblank_slices--;
    } else
        rr_dsp_run(DSP_STEPS_PER_FRAME / SLICES);
    rr_gles3_stage_end(RR_STAGE_DSP,stage_start); stage_start=rr_gles3_stage_begin();
    rr_sim_enter(SIM_SOUND);
    rr_sound_slice();                              /* the sound program's share of this slice */
    rr_gles3_stage_end(RR_STAGE_SOUND,stage_start); stage_start=rr_gles3_stage_begin();
    rr_sim_enter(SIM_AUDIO);
    rr_audio_slice();                              /* and the mixer's samples */
    rr_gles3_stage_end(RR_STAGE_AUDIO,stage_start); stage_start=rr_gles3_stage_begin();
    rr_sim_enter(SIM_SERIAL);
    serial_acc += 100;                             /* 100 Hz serial pulse */
    if (serial_acc >= 60 * SLICES) { serial_acc -= 60 * SLICES; rr_dsp_serial(); }
    rr_gles3_stage_end(RR_STAGE_SERIAL,stage_start);
    rr_sim_enter(SIM_SCHEDULER);
    if (++slice % SLICES) {
        rr_sim_leave(sim_parent);
        return;
    }
    frame++;
    rr_sim_pause(frame);
    if(!rr_presentation_video(frame,rr_dsp_slave_active())){
        rr_headless_summary();rr_audio_close();rr_gles3_close();
        exit(rr_gles3_failed()?2:0);
    }
    rr_sim_resume(frame+1);
    for (int c = 0; c < n_test_coins; c++) {       /* scripted inputs for headless captures */
        if (frame == (uint32_t)test_coins[c]) g_hw.inputs &= (uint16_t)~0x1000;
        if (frame == (uint32_t)test_coins[c] + 6) g_hw.inputs |= 0x1000;
    }
    if (test_gas >= 0 && frame >= (uint32_t)test_gas) g_hw.gas = (uint16_t)g_rr_game->gas_max;
    for (int i = 0; i < n_steer; i++) if (frame >= (uint32_t)test_steer[i].at) g_hw.steer = (uint16_t)test_steer[i].value;
    /* Interactive input lands at the same frame boundary as desktop controls. */
    if(!rr_gles3_record_exit_pending())rr_gles3_apply_input();
    if(replay_session)rr_input_frame(frame);
    if(replay_session && !rr_input_stream_ok()){fprintf(stderr,"[REPLAY] input stream error\n");exit(2);}
    stage_start=rr_gles3_stage_begin();
    rr_sim_enter(SIM_BOARD);
    vblank_slices = 2;
    rr_dsp_vblank();
    if (!rr_env_active()) rr_hw_vblank();      /* (a trace oracle refreshes at MAME's own vblank interrupt: rr_env.c) */
    rr_link_poll();                      /* a received link packet's SCI IRQ lands at this frame edge */
    rr_gles3_stage_end(RR_STAGE_IO,stage_start);
    rr_sim_enter(SIM_IRQ);
    if (!rr_env_active()) {                  /* a trace oracle lands the interrupts itself */
        rr_deliver_irqs(rr_hw_irq_level);
        /* online: the other pending rivals' packets, each through the game's own SCI handler (offline nothing is pending) */
        for (int k = 0; k < 7; k++) {
            if (g_hw.irq_state & ~(1u << 2)) break;      /* only while the SCI is all that is pending: never re-run another, unacknowledged handler */
            if (!rr_link_inject_next()) break;
            rr_deliver_irqs(rr_hw_irq_level);
        }
    }
    rr_sim_enter(SIM_SCHEDULER);
    if (frame % 60 == 0)
        fprintf(stderr, "[RR] frame %u  traps %u  unmapped %u  romwrites %u  irqs %u/%u/%u/%u/%u/%u/%u  en %02X pc_sr %04X\n",
                frame, rr_n_traps, g_rr.n_unmapped, g_rr.n_romwrite, rr_n_irq[1], rr_n_irq[2], rr_n_irq[3], rr_n_irq[4], rr_n_irq[5], rr_n_irq[6], rr_n_irq[7],
                g_hw.irq_enabled, (unsigned)get_sr());
    if (frame % 60 == 0) { char b[256]; rr_dsp_debug(b, sizeof b); fprintf(stderr, "     %s\n", b); }
    /* Finish a recording's last tick instead of terminating inside input polling. */
    if(rr_gles3_record_exit_pending())max_frames=frame;
    /* Optional per-tick audio evidence, independent of the historical sidecar. */
    { static FILE *audio_reference; static bool opened;
      if (!opened) {
        const char *p=getenv("ENG_SOUND_REFERENCE"); opened=true;
        if (p && *p) { audio_reference=fopen(p,"wx"); if(!audio_reference){perror(p);exit(2);} }
      }
      if(audio_reference) { rr_audio_checkpoint(audio_reference,frame);
        if(fflush(audio_reference)){perror("audio reference");exit(2);} }
    }
    completed_replay_frame=frame;
    if(replay_timing_file) {
        struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
        fprintf(replay_timing_file,"%u,%.9f\n",frame,t.tv_sec+t.tv_nsec/1e9);
        if(ferror(replay_timing_file))exit(2);
    }
    if(checkpoint_file && (frame%300==0 || frame==max_frames)) {
        rr_headless_state(checkpoint_file);
        rr_replay_board_checkpoint(checkpoint_file);
        rr_audio_checkpoint(checkpoint_file,frame);
        if(ferror(checkpoint_file)){fprintf(stderr,"[REPLAY] checkpoint write failed\n");exit(2);}
    }
    if (frame >= max_frames) {
        rr_headless_summary();
        rr_audio_close();
        rr_gles3_close();
        if (rr_gles3_failed()) exit(2);
        fprintf(stderr, "[RR] stop at frame %u, %u traps\n", frame, rr_n_traps);
        exit(rr_n_traps || rr_headless_dsp_fault() || rr_headless_sound_fault() ? 3 : 0);
    }
    rr_sim_leave(sim_parent);
}

int main(int argc, char **argv)
{
    return rr_headless_main(argc, argv);
}
