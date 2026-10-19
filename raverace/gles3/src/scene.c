/*
 * rr_scene.c -- System 22 video front end (see include/rr_scene.h).
 * The render_frame_active rule is namcos22_state::pdp_begin_r /
 * render_frame_active in MAME's namcos22.cpp: the list is kept for a screen
 * update when the master began its frame during the vblank before it (the
 * frame counter it recorded is ahead), and dropped once the master has since
 * asked for a render refresh.
 */
#include <stdio.h>
#include "renderer_api.h"
#include "lift_cpu.h"
#include <string.h>
#include "rr_mem.h"
#include "rr_scene.h"

#define DIRECT_MAX 4096
static uint16_t direct_q[DIRECT_MAX][0x1C];
static int      direct_n;
static bool     pdp_render_done, render_refresh;
static uint64_t pdp_frame, cur_frame;

void rr_scene_pdp_begin(bool in_vblank) { pdp_frame = cur_frame + (in_vblank ? 1 : 0); pdp_render_done = true; }
void rr_scene_render_refresh(void)      { render_refresh = true; }

void rr_scene_direct_poly(const uint16_t *src)
{
    if(direct_n>=DIRECT_MAX)gles3_unsupported(rr_frame,
      (g_rr.wram[0x8da0]<<8)|g_rr.wram[0x8da1],(g_rr.wram[0xa32e]<<8)|g_rr.wram[0xa32f],
      -1,direct_n,NULL,"direct command FIFO capacity");
    memcpy(direct_q[direct_n++],src,sizeof direct_q[0]);
}

bool rr_scene_frame(bool slave_active)
{
    cur_frame++;
    if (cur_frame > pdp_frame && render_refresh) pdp_render_done = false;
    render_refresh = false;
    return pdp_render_done && slave_active;
}

int  rr_scene_direct_count(void)   { return direct_n; }
const uint16_t *rr_scene_direct(int i) { return direct_q[i]; }
void rr_scene_consume(void)        { direct_n = 0; }

void rr_scene_force_walk(void) { pdp_render_done = true; render_refresh = false; pdp_frame = cur_frame + 1; }

