/* Direct tests of the real rr_mem.c: callback/probe/checker seams are stubs,
 * not replacements for game execution. Build baseline + fast from same source. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rr_mem.h"
#include "rr_game.h"
const rr_game_t *g_rr_game;
int rd_read_log,rd_quiet=1,rd_io_written,rd_io_touched,rd_ior_replay;
extern int rd_journal_on;
static unsigned callbacks,probes,logged,replayed,bytes,journal_bytes,device_reads;
void rd_io_log(vaddr_t a,int n,uint32_t v) {(void)a;(void)n;(void)v;}
void rd_ior_log(vaddr_t a,int n,uint32_t v) {(void)a;(void)n;(void)v;logged++;}
uint32_t rd_ior_serve(vaddr_t a,int n) {(void)a;(void)n;replayed++;return 0x12345678;}
void rd_read_byte(vaddr_t a,uint8_t *p) {(void)a;(void)p;bytes++;}
void rd_dict_byte(uint8_t v) {(void)v;bytes++;}
void rd_journal_byte(vaddr_t a,uint8_t *p) {(void)a;(void)p;journal_bytes++;}
#ifdef RR_TRACE
uint32_t rr_trace_pc;
extern void (*rr_read_probe)(vaddr_t,int);
static void probe(vaddr_t a,int n) {(void)a;(void)n;probes++;}
#endif
static bool reject(vaddr_t a,int n,uint32_t *v)
{ (void)a;(void)n;(void)v;callbacks++;return false; }
static bool override(vaddr_t a,int n,uint32_t *v)
{ (void)n;callbacks++;if(a==0x10000003u){*v=0xaabbccdd;return true;}return false; }
static bool dirty_reject(vaddr_t a,int n,uint32_t *v)
{ (void)a;(void)n;callbacks++;*v=0x1234;return false; }
static bool device(vaddr_t a,int n,uint32_t *v)
{ (void)n;callbacks++;if(a==0x50000008u){*v=0x10000+(++device_reads);return true;}return false; }
static uint32_t be(const uint8_t *p,int n) {uint32_t v=0;while(n--)v=v<<8|*p++;return v;}
static void region(uint8_t *p,uint32_t base,uint32_t length)
{
 for(uint32_t i=0;i<length;i++)p[i]=(uint8_t)(i*37+19);
 for(int n=1;n<=4;n++) { /* size 3 must also preserve original fallback */
  for(unsigned i=0;i<17;i++)assert(rr_read(base+i,n)==be(p+i,n));
  assert(rr_read(base+length-n,n)==be(p+length-n,n));
  unsigned faults=g_rr.n_unmapped;
  for(unsigned crossing=1;crossing<(unsigned)n;crossing++)assert(rr_read(base+length-crossing,n)==0);
  assert(g_rr.n_unmapped==faults+(unsigned)n-1);
 }
}
int main(void)
{
 rr_set_io_hooks(NULL,NULL);
 region(g_rr.wram,0x10000000u,RR_WRAM_SIZE);
 region(g_rr.rom,0,RR_ROM_SIZE);
 region(g_rr.shared,0x60004000u,RR_SHARED_SIZE);
 for(unsigned n=1;n<=4;n++)for(unsigned i=0;i<17;i++)assert(rr_read(0x18000000u+i,n)==rr_read(0x10000000u+i,n));
 unsigned faults=g_rr.n_unmapped;
 assert(rr_read(0x10020000u,1)==0);assert(rr_read(0x18020000u,4)==0);
 assert(rr_read(0x10800000u,2)==0);assert(rr_read(0x30000000u,4)==0);
 assert(g_rr.n_unmapped==faults+4);
 assert(rr_read(0x10000000u,0)==0);
 assert(rr_read(0x10000001u,8)==be(g_rr.wram+1,8)); /* legacy uint32 truncation */
 faults=g_rr.n_unmapped;
 assert(rr_read(0x1001fff9u,8)==0);assert(g_rr.n_unmapped==faults+1);
 faults=g_rr.n_unmapped;
 assert(rr_read(0x10000000u,-1)==0);assert(g_rr.n_unmapped==faults+1);
 faults=g_rr.n_unmapped;
 assert(rr_read(0x10000001u,-1)==0);assert(g_rr.n_unmapped==faults); /* legacy unsigned-bound quirk */
 rr_set_io_hooks(override,NULL);callbacks=0;
 assert(rr_read(0x10000003u,4)==0xaabbccdd);assert(callbacks==1);
 /* Unknown hooks must execute even if they reject ordinary RAM. */
 rr_set_io_hooks(reject,NULL);callbacks=0;
 assert(rr_read(0x10000001u,2)==be(g_rr.wram+1,2));assert(callbacks==1);
 /* False return alone does not establish purity; preserve an unknown hook's
  * original output-accumulator effect, and revoke a previous certification. */
 rr_set_plain_wram_io_hooks(reject,NULL);
 rr_set_io_hooks(dirty_reject,NULL);callbacks=0;
 assert(rr_read(0x10000002u,1)==(0x123400u|g_rr.wram[2]));assert(callbacks==1);
 rr_set_plain_wram_io_hooks(device,NULL);callbacks=0;device_reads=0;
 uint32_t first=rr_read(0x50000008u,1),second=rr_read(0x50000008u,1);
 assert(second==first+1 && callbacks==2);
 assert(rr_read(0x10000001u,4)==be(g_rr.wram+1,4));
#ifdef RR_FAST_MEMORY_READS
 assert(callbacks==2);
#else
 assert(callbacks==3);
#endif
 /* Ordinary WRAM rejection is pure: future device side effects stay identical. */
 assert(rr_read(0x50000008u,2)==second+1 && device_reads==3);
 /* Crossing read must still invoke original callback and fault. */
 callbacks=0;faults=g_rr.n_unmapped;assert(rr_read(0x1001ffffu,2)==0);
 assert(callbacks==1 && g_rr.n_unmapped==faults+1);
 rd_journal_on=1;callbacks=0;assert(rr_read(0x10000001u,4)==be(g_rr.wram+1,4));assert(callbacks==1);
 rd_read_log=1;bytes=0;assert(rr_read(0x10000002u,4)==be(g_rr.wram+2,4));assert(bytes==4);
 rd_read_log=0;rd_io_written=1;rd_io_touched=0;logged=0;
 assert(rr_read(0x50000008u,1)!=0);assert(logged==1 && rd_io_touched==1);
 rd_ior_replay=1;replayed=0;callbacks=0;assert(rr_read(0x50000008u,2)==0x12345678);
 assert(replayed==1 && callbacks==0);
 rd_ior_replay=0;rd_journal_on=0;rd_io_written=0;
#ifdef RR_TRACE
 rr_read_probe=probe;probes=0;callbacks=0;
 assert(rr_read(0x10000001u,2)==be(g_rr.wram+1,2));assert(probes==1 && callbacks==1);
 rr_read_probe=NULL;
#endif
 rr_set_io_hooks(NULL,NULL);g_rr.poly[1]=0x00801234;
 assert(rr_read(0x70000004u,4)==0xff801234);
 assert(rr_read(0x70000005u,2)==0xff80); /* original polygon alignment */
 assert(rr_read(0x70000007u,1)==0x34);
 /* Current CGRAM/TEXT constants are adjacent, despite an old overlap comment. */
 g_rr.text[0]=0x91;g_rr.cgram[RR_CGRAM_SIZE-1]=0x42;
 assert(rr_read(0x9009e000u,1)==0x91);
 assert(rr_read(0x90080000u+RR_CGRAM_SIZE-1,1)==0x42);
 faults=g_rr.n_unmapped;
 assert(rr_read(0x90080000u+RR_CGRAM_SIZE-1,2)==0);
 assert(g_rr.n_unmapped==faults+1);
 unsigned romwrites=g_rr.n_romwrite;rr_write(0,1,0xee);assert(g_rr.n_romwrite==romwrites+1);
 rr_write(0x10000001u,4,0x89abcdef);assert(rr_read(0x18000001u,4)==0x89abcdef);
 rd_journal_on=1;journal_bytes=0;rr_write(0x10000003u,2,0x1122);assert(journal_bytes==2);rd_journal_on=0;
 /* Wide/random contained loads, both aliases, and live write visibility. */
 uint32_t seed=0x87654321;
 for(unsigned i=0;i<20000;i++) {
  seed=seed*1664525+1013904223;unsigned off=seed%(RR_WRAM_SIZE-4);
  int n=(i%3)==0?1:(i%3)==1?2:4;
  uint32_t a=(i&1?0x10000000u:0x18000000u)+off;
  assert(rr_read(a,n)==be(g_rr.wram+off,n));
 }
 rd_quiet=0;faults=g_rr.n_unmapped;
 assert(rr_read(0x30001000u,1)==0);assert(rr_read(0x30001001u,1)==0);
 assert(g_rr.n_unmapped==faults+2); /* one deduplicated diagnostic, two faults */
 puts("Real memory mapping, widths/unaligned/boundaries/mirrors, callbacks, probes, checker, polygon behavior, adjacent regions, faults and live writes passed");
 return 0;
}
