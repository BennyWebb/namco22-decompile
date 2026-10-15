/* Isolated 20 Hz telemetry, one long-lived process, no fork/exec.
 * Raw sysfs values retained; numeric trans_stat deltas are diagnostic vectors
 * (frequency labels have zero delta; counters retain the driver's own units). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
static char buf[65536];
static unsigned long long prior[4096];static size_t pn;
static int read_file(const char*p){FILE*f=fopen(p,"r");if(!f){printf("MISSING %s %d\n",p,errno);return 0;}size_t n=fread(buf,1,sizeof(buf)-1,f);buf[n]=0;fclose(f);printf("FILE %s\n%s%sEND\n",p,buf,n&&buf[n-1]=='\n'?"":"\n");return 1;}
static void filtered(const char*p){FILE*f=fopen(p,"r");if(!f)return;printf("FILE %s\n",p);char line[512];while(fgets(line,sizeof line,f))if(!strncmp(line,"MemAvailable:",13)||!strncmp(line,"VmRSS:",6)||!strncmp(line,"voluntary_ctxt_switches:",23)||!strncmp(line,"nonvoluntary_ctxt_switches:",26)||!strncmp(line,"Cpus_allowed_list:",18))fputs(line,stdout);fclose(f);puts("END");}
static void trans(void){if(!read_file("/sys/class/devfreq/gpu/trans_stat"))return;unsigned long long now[4096];size_t n=0;char*p=buf;while(*p&&n<4096){if(isdigit((unsigned char)*p)){char*e;now[n++]=strtoull(p,&e,10);p=e;}else p++;}printf("TRANS_DELTA valid=%d count=%zu",pn==n&&pn!=0,n);for(size_t i=0;i<n;i++){if(pn==n&&pn)printf(" %lld",(long long)(now[i]-prior[i]));prior[i]=now[i];}pn=n;putchar('\n');}
int main(int argc,char**argv){if(argc!=2)return 2;int pid=atoi(argv[1]);if(pid<2)return 2;setvbuf(stdout,NULL,_IOFBF,262144);printf("HZ %ld\nINTERVAL_NS 50000000\n",sysconf(_SC_CLK_TCK));struct timespec next;clock_gettime(CLOCK_MONOTONIC,&next);while(!kill(pid,0)||errno==EPERM){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);printf("SAMPLE %.9f\n",t.tv_sec+t.tv_nsec/1e9);read_file("/sys/class/devfreq/gpu/cur_freq");read_file("/sys/class/devfreq/gpu/governor");trans();read_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");for(int c=0;c<2;c++){char cooling[128];snprintf(cooling,sizeof cooling,"/sys/class/thermal/cooling_device%d/cur_state",c);read_file(cooling);}struct timespec own;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&own);printf("SAMPLER_CPU %.9f\n",own.tv_sec+own.tv_nsec/1e9);char p[256];for(int i=0;i<4;i++){snprintf(p,sizeof p,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",i);read_file(p);}DIR*d=opendir("/sys/class/thermal");struct dirent*e;if(d){while((e=readdir(d)))if(!strncmp(e->d_name,"thermal_zone",12)){snprintf(p,sizeof p,"/sys/class/thermal/%.120s/temp",e->d_name);read_file(p);}closedir(d);}snprintf(p,sizeof p,"/proc/%d/status",pid);filtered(p);filtered("/proc/meminfo");read_file("/sys/kernel/debug/mali0/gpu_memory");snprintf(p,sizeof p,"/proc/%d/task",pid);d=opendir(p);if(d){while((e=readdir(d)))if(atoi(e->d_name)>0){snprintf(p,sizeof p,"/proc/%d/task/%d/stat",pid,atoi(e->d_name));read_file(p);snprintf(p,sizeof p,"/proc/%d/task/%d/status",pid,atoi(e->d_name));filtered(p);}closedir(d);}fflush(stdout);next.tv_nsec+=50000000;if(next.tv_nsec>=1000000000){next.tv_nsec-=1000000000;next.tv_sec++;}clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&next,NULL);}return 0;}
