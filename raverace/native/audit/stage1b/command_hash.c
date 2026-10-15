/* Opt-in validation-only command packet receipt. Compiler/binary unmodified.
 * RR_COMMAND_HASH=csv RR_COMMAND_DIR=dump-prefix LD_PRELOAD=command_hash.so
 * Intercepts ONLY native packet dump fopen paths, hashes every byte through a
 * normal stdio cookie, retains SHA256+length, avoids writing ROM-derived packets
 * to flash. Never loaded/enabled in performance runs. Default OFF.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <openssl/sha.h>
static FILE*receipt;static char*prefix;static size_t plen;static unsigned char seen[8192];
typedef struct{SHA256_CTX sha;unsigned tick;size_t bytes;} packet;
static FILE*original_open(const char*p,const char*m,const char*symbol){FILE*(*fn)(const char*,const char*)=dlsym(RTLD_NEXT,symbol);if(!fn)_exit(95);return fn(p,m);}
static void finish(void){if(receipt&&fclose(receipt))_exit(95);receipt=NULL;}
__attribute__((constructor))static void init(void){const char*out=getenv("RR_COMMAND_HASH"),*dir=getenv("RR_COMMAND_DIR");if(!out||!*out)return;if(!dir||!*dir)_exit(95);prefix=strdup(dir);if(!prefix)_exit(95);plen=strlen(prefix);receipt=original_open(out,"wx","fopen");if(!receipt)_exit(95);fputs("tick,bytes,sha256\n",receipt);fflush(receipt);atexit(finish);}
static ssize_t append(void*cookie,const char*bytes,size_t n){packet*p=cookie;if(SHA256_Update(&p->sha,bytes,n)!=1){errno=EIO;return -1;}p->bytes+=n;return (ssize_t)n;}
static int close_packet(void*cookie){packet*p=cookie;unsigned char digest[32];if(SHA256_Final(digest,&p->sha)!=1){free(p);errno=EIO;return -1;}fprintf(receipt,"%u,%zu,",p->tick,p->bytes);for(int i=0;i<32;i++)fprintf(receipt,"%02x",digest[i]);fputc('\n',receipt);int result=fflush(receipt);free(p);return result;}
static FILE*open_packet(const char*path,const char*mode,const char*symbol){if(!receipt||strncmp(path,prefix,plen)||path[plen]!='/')return original_open(path,mode,symbol);unsigned tick;int used=0;if(sscanf(path+plen+1,"tick-%u.live%n",&tick,&used)!=1||path[plen+1+used]||(strcmp(mode,"wb")&&strcmp(mode,"wbx"))||tick>=8192){errno=EINVAL;return NULL;}if(strchr(mode,'x')&&(seen[tick]||access(path,F_OK)==0)){errno=EEXIST;return NULL;}packet*p=calloc(1,sizeof *p);if(!p)return NULL;p->tick=tick;if(SHA256_Init(&p->sha)!=1){free(p);return NULL;}cookie_io_functions_t io={.read=NULL,.write=append,.seek=NULL,.close=close_packet};FILE*f=fopencookie(p,"w",io);if(!f)free(p);else seen[tick]=1;return f;}
FILE*fopen(const char*p,const char*m){return open_packet(p,m,"fopen");}
FILE*fopen64(const char*p,const char*m){return open_packet(p,m,"fopen64");}
