/* Stage1: one persistent /proc sampler; no affinity/scheduler changes. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <string.h>
static void copy(const char*p){char b[4096];FILE*f=fopen(p,"r");if(!f)return;while(fgets(b,sizeof b,f))fputs(b,stdout);fclose(f);}
int main(int argc,char**argv){if(argc!=2)return 2;int pid=atoi(argv[1]);if(pid<=1)return 2;printf("HZ=%ld\n",sysconf(_SC_CLK_TCK));char path[256];struct timespec delay={0,500000000};while(!kill(pid,0)||errno==EPERM){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);printf("\nSAMPLE %.9f\n",t.tv_sec+t.tv_nsec/1e9);snprintf(path,sizeof path,"/proc/%d/task",pid);DIR*d=opendir(path);if(d){struct dirent*e;while((e=readdir(d)))if(atoi(e->d_name)>0){snprintf(path,sizeof path,"/proc/%d/task/%d/stat",pid,atoi(e->d_name));fputs("TASK ",stdout);copy(path);}closedir(d);}snprintf(path,sizeof path,"/proc/%d/status",pid);copy(path);copy("/sys/kernel/debug/mali0/gpu_memory");copy("/proc/meminfo");fputs("FREQ ",stdout);copy("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");fputs("TEMP ",stdout);copy("/sys/class/thermal/thermal_zone0/temp");fflush(stdout);nanosleep(&delay,0);}return 0;}
