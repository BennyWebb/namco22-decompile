/* One persistent diagnostic process; no fork/exec in the sampling loop. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
static void copy(const char *p){FILE*f=fopen(p,"r");if(!f)return;char b[4096];while(fgets(b,sizeof b,f))fputs(b,stdout);fclose(f);}
int main(int argc,char**argv){if(argc!=2)return 2;int pid=atoi(argv[1]);if(pid<=1)return 2;char path[128];snprintf(path,sizeof path,"/proc/%d/status",pid);struct timespec wait={1,0};while(!kill(pid,0)||errno==EPERM){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);printf("\nSAMPLE %.6f\n",t.tv_sec+t.tv_nsec/1e9);copy(path);copy("/sys/kernel/debug/mali0/gpu_memory");copy("/proc/meminfo");fputs("frequency_khz=",stdout);copy("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");fputs("temperature_millidegrees=",stdout);copy("/sys/class/thermal/thermal_zone0/temp");fflush(stdout);nanosleep(&wait,0);}return 0;}
