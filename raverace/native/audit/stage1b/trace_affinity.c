/* Short attribution/validation only. Never used for timed game runs. */
#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#if defined(__aarch64__)
#include <asm/ptrace.h>
#else
#include <sys/user.h>
#endif
int main(int argc,char**argv){if(argc<2)return 2;pid_t child=fork();if(child<0)return 2;if(!child){if(ptrace(PTRACE_TRACEME,0,0,0)){perror("PTRACE_TRACEME");_exit(90);}raise(SIGSTOP);execvp(argv[1],argv+1);_exit(91);}int status;waitpid(child,&status,0);if(!WIFSTOPPED(status)){fprintf(stderr,"tracing unavailable\n");return 77;}ptrace(PTRACE_SETOPTIONS,child,0,PTRACE_O_TRACESYSGOOD|PTRACE_O_TRACECLONE|PTRACE_O_EXITKILL);ptrace(PTRACE_SYSCALL,child,0,0);unsigned stops=0,syscalls=0;int exitcode=-1;pid_t pid;while((pid=waitpid(-1,&status,__WALL))>0){if(WIFEXITED(status)||WIFSIGNALED(status)){if(pid==child)exitcode=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);continue;}if(WIFSTOPPED(status)){int sig=WSTOPSIG(status);if(sig==(SIGTRAP|0x80)){syscalls++;
#if defined(__aarch64__)
 struct user_pt_regs r;struct iovec iov={&r,sizeof r};if(!ptrace(PTRACE_GETREGSET,pid,(void*)NT_PRSTATUS,&iov)&&r.regs[8]==SYS_sched_setaffinity)stops++;
#else
 struct user_regs_struct r;if(!ptrace(PTRACE_GETREGS,pid,0,&r)&&r.orig_rax==SYS_sched_setaffinity)stops++;
#endif
}ptrace(PTRACE_SYSCALL,pid,0,(sig==SIGTRAP||sig==(SIGTRAP|0x80)||sig==SIGSTOP)?0:sig);}}printf("syscall_stops=%u sched_setaffinity_entry_and_exit_stops=%u child_exit=%d\n",syscalls,stops,exitcode);return exitcode;}
