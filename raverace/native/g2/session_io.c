/* Session-only transport adapter: fopen("wx") must duplicate the inherited pipe,
 * not request exclusive creation of an already-existing /proc fd pathname. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
int open(const char*p,int flags,...){mode_t mode=0;if(flags&O_CREAT){va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);}if(getenv("ENG_G2_SESSION")&&!strncmp(p,"/proc/self/fd/",14)){const char*s=p+14;char*end;long n=strtol(s,&end,10);if(*s&&!*end&&n>=0)return fcntl((int)n,F_DUPFD_CLOEXEC,3);}return syscall(SYS_openat,AT_FDCWD,p,flags,mode);}
