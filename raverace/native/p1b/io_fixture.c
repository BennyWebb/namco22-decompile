/* Host-only pressure fixture: full rings must drop rather than block producers;
 * reserved checkpoint data must survive shutdown drain byte-for-byte. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
static double stamp(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char**argv){if(argc!=2)return 2;static unsigned char bytes[65536];memset(bytes,0xa5,sizeof bytes);double start=stamp(),worst=0;for(int i=0;i<1024;i++){double t=stamp();if(fwrite(bytes,1,sizeof bytes,stdout)!=sizeof bytes)return 3;double dt=stamp()-t;if(dt>worst)worst=dt;}FILE*f=fopen(argv[1],"wx");if(!f)return 4;for(int i=0;i<128;i++)if(fwrite(bytes,1,1024,f)!=1024)return 5;if(fclose(f))return 6;fprintf(stderr,"[IO-FIXTURE] produced_bytes=67108864 elapsed_ms=%.6f max_append_ms=%.6f checkpoint_bytes=131072\n",(stamp()-start)*1000,worst*1000);return 0;}
