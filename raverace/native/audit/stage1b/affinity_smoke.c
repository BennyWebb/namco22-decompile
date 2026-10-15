#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void report(const char*n){cpu_set_t s;CPU_ZERO(&s);pthread_getaffinity_np(pthread_self(),sizeof s,&s);printf("%s:",n);for(int c=0;c<CPU_SETSIZE;c++)if(CPU_ISSET(c,&s))printf(" %d",c);putchar('\n');}
static void*worker(void*p){const char*n=p;pthread_setname_np(pthread_self(),n);report(n);return NULL;}
int main(void){usleep(200000);report("main");const char*names[]={"rr-prepare","rr-present","audio-helper"};for(int i=0;i<3;i++){pthread_t t;if(pthread_create(&t,NULL,worker,(void*)names[i]))return 2;pthread_join(t,NULL);}usleep(200000);return 0;}
