#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
int main(void){const char*d=getenv("RR_COMMAND_DIR");if(!d)return 2;usleep(200000);for(unsigned tick=1;tick<=3;tick++){char path[1024];snprintf(path,sizeof path,"%s/tick-%u.live",d,tick);FILE*f=fopen(path,"wbx");if(!f)return 2;if(tick==1&&fwrite("abc",1,3,f)!=3)return 2;if(tick==3){unsigned char block[4096];for(unsigned i=0;i<sizeof block;i++)block[i]=(unsigned char)(i*37+11);for(int i=0;i<256;i++)if(fwrite(block,1,sizeof block,f)!=sizeof block)return 2;}if(fclose(f))return 2;}usleep(200000);return 0;}
