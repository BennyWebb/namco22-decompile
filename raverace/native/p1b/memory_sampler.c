/* Low-priority, no PSI reads, 10Hz. All output uses async_io memory cookies.
 * Per-read monotonic bounds disambiguate slow observers. No catch-up bursts. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>
static volatile sig_atomic_t stopping;static void stop(int s){(void)s;stopping=1;}
static double stamp(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static char data[65536];
static int readfile(const char*path,int filter){double start=stamp();FILE*f=fopen(path,"r");if(!f){printf("MISSING %s %d\n",path,errno);return 0;}size_t n=fread(data,1,sizeof data-1,f);data[n]=0;fclose(f);double end=stamp();printf("READ_BOUNDS %.9f %.9f %s\nFILE %s\n",start,end,path,path);
 if(filter){char*save,*line=strtok_r(data,"\n",&save);while(line){if(strstr(line,"VmRSS:")||strstr(line,"VmHWM:")||strstr(line,"MemAvailable:")||strstr(line,"CmaFree:")||!strncmp(line,"pgmajfault ",11)||!strncmp(line,"pgscan",6)||!strncmp(line,"pgsteal",7)||!strncmp(line,"allocstall",10))puts(line);line=strtok_r(NULL,"\n",&save);}}
 else {printf("%s%s",data,n&&data[n-1]!='\n'?"\n":"");}puts("END");return 1;}
int main(int argc,char**argv){if(argc<2||argc>3)return 2;unsetenv("ENG_P1B_RECEIPTS");unsetenv("ENG_P1B_IMAGE_HASH");int parent=atoi(argv[1]),pid=parent;int follow=argc==3;if(pid<2)return 2;signal(SIGTERM,stop);signal(SIGINT,stop);setpriority(PRIO_PROCESS,0,19);struct sched_param sp={0};int r=sched_setscheduler(0,SCHED_IDLE,&sp);printf("P1B_OBSERVER interval_ms=100 PSI=OFF SCHED_IDLE_result=%d nice=%d\n",r,getpriority(PRIO_PROCESS,0));double next=stamp();
 while(!stopping&&(!kill(parent,0)||errno==EPERM)){
  pid=parent;if(follow){char children[128];snprintf(children,sizeof children,"/proc/%d/task/%d/children",parent,parent);FILE*cf=fopen(children,"r");if(cf){int candidate;while(fscanf(cf,"%d",&candidate)==1){char cp[128],comm[64]={0};snprintf(cp,sizeof cp,"/proc/%d/comm",candidate);FILE*f=fopen(cp,"r");if(f){fgets(comm,sizeof comm,f);fclose(f);if(!strcmp(comm,"rr_p1b\n")){pid=candidate;break;}}}fclose(cf);}}

  double start=stamp();printf("SAMPLE %.9f\nTARGET_PID %d\n",start,pid);readfile("/sys/class/devfreq/gpu/cur_freq",0);readfile("/sys/class/devfreq/gpu/governor",0);readfile("/sys/class/devfreq/gpu/trans_stat",0);
  char path[256];for(int i=0;i<4;i++){snprintf(path,sizeof path,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",i);readfile(path,0);}
  DIR*d=opendir("/sys/class/thermal");struct dirent*e;if(d){while((e=readdir(d)))if(!strncmp(e->d_name,"thermal_zone",12)){snprintf(path,sizeof path,"/sys/class/thermal/%.120s/temp",e->d_name);readfile(path,0);}else if(!strncmp(e->d_name,"cooling_device",14)){snprintf(path,sizeof path,"/sys/class/thermal/%.120s/cur_state",e->d_name);readfile(path,0);}closedir(d);}
  snprintf(path,sizeof path,"/proc/%d/status",pid);readfile(path,1);readfile("/proc/meminfo",1);readfile("/proc/vmstat",1);snprintf(path,sizeof path,"/proc/%d/io",pid);readfile(path,0);readfile("/sys/kernel/debug/mali0/gpu_memory",0);
  snprintf(path,sizeof path,"/proc/%d/task",pid);d=opendir(path);if(d){while((e=readdir(d))){int tid=atoi(e->d_name);if(tid<1)continue;snprintf(path,sizeof path,"/proc/%d/task/%d/comm",pid,tid);FILE*f=fopen(path,"r");if(!f)continue;char comm[64]={0};fgets(comm,sizeof comm,f);fclose(f);if(strcmp(comm,"rr_p1b\n")&&strcmp(comm,"rr-prepare\n")&&strcmp(comm,"rr-present\n"))continue;
   snprintf(path,sizeof path,"/proc/%d/task/%d/stat",pid,tid);readfile(path,0);char*state=strrchr(data,')');int blocked=state&&state[2]=='D';snprintf(path,sizeof path,"/proc/%d/task/%d/wchan",pid,tid);readfile(path,0);{snprintf(path,sizeof path,"/proc/%d/task/%d/stack",pid,tid);readfile(path,0);}
  }closedir(d);}
  struct timespec own;struct rusage usage;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&own);getrusage(RUSAGE_SELF,&usage);printf("SAMPLER_CPU %.9f SAMPLER_MAXRSS_KiB %ld\n",own.tv_sec+own.tv_nsec*1e-9,usage.ru_maxrss);double end=stamp();printf("SAMPLE_END %.9f duration_ms=%.6f\n",end,(end-start)*1000);next+=.1;if(next<end){printf("OBSERVER_LATE %.9f lag_ms=%.6f\n",end,(end-next)*1000);next=end+.1;}struct timespec deadline={(time_t)next,(long)((next-(time_t)next)*1e9)};clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&deadline,NULL);
 }return 0;}
