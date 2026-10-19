/* Interpreter executor for the sound coverage and PCM/voice reference gates. */
#include "snd_board.h"
#include <stdio.h>
void snd_executor_init(void) { fprintf(stderr, "[SND] memory-fetch interpreter oracle\n"); }
uint64_t snd_spin_fast(m37710_t *c, uint64_t limit) { (void)c; (void)limit; return 0; }
void snd_execute(m37710_t *c, uint64_t end)
{
    snd_board_port4();
    while (c->cycles < end && !c->unimpl_hit) {
        uint64_t rem = end - c->cycles;
        m37710_run(c, (int)(rem < 64 ? rem : 64));
    }
}
