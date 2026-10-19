#ifndef RR_SIM_PROFILE_H
#define RR_SIM_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { SIM_CORE, SIM_SCHEDULER, SIM_DSP, SIM_SOUND, SIM_AUDIO,
    SIM_SERIAL, SIM_BOARD, SIM_IRQ, SIM_VIDEO, SIM_PRESENT, SIM_COUNT } rr_sim_stage;
bool rr_sim_options(int *argc, char **argv);
bool rr_sim_start(void);
rr_sim_stage rr_sim_enter(rr_sim_stage stage);
void rr_sim_leave(rr_sim_stage stage);
void rr_sim_pause(uint32_t frame);
void rr_sim_resume(uint32_t next_frame);
void rr_sim_report(void);
void rr_sim_present_tick(uint32_t frame, bool rendered);
#endif
