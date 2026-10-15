/* Offline transfer adapter for initial 8-vec4 -> final 9-vec4 command records.
 * Original rendering fields remain byte identical; adds captured identity only.
 * Run on diagnostic files, never production. Final output hashes must validate. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#define C(x) do{if(!(x))abort();}while(0)
int main(int argc,char**argv){C(argc==4);gzFile ids=gzopen(argv[3],"rb");C(ids);char dest[4096];snprintf(dest,sizeof dest,"%s/frames.rsc",argv[2]);FILE*o=fopen(dest,"wbx");C(o);uint32_t container[]={0x33525352,2,3840,0};uint64_t offsets[3841]={0};C(fwrite(container,4,4,o)==4&&fwrite(offsets,8,3841,o)==3841);
for(unsigned tick=1800;tick<=5639;tick++){offsets[tick-1800]=ftell(o);uint32_t ih[2];C(gzread(ids,ih,8)==8&&ih[0]==tick&&ih[1]<=4096);char old[4096];snprintf(old,sizeof old,"%s/frames/%u.bin",argv[1],tick);FILE*f=fopen(old,"rb");C(f);uint32_t h[11];C(fread(h,4,11,f)==11&&h[2]==tick&&h[3]==ih[1]&&h[1]==1);h[1]=2;C(fwrite(h,4,11,o)==11);size_t n=h[3];float*commands=malloc(n*128);C(commands&&fread(commands,128,n,f)==n);for(size_t i=0;i<n;i++){float id[4];C(gzread(ids,id,16)==16);C(fwrite(commands+i*32,128,1,o)==1&&fwrite(id,16,1,o)==1);}free(commands);unsigned char b[65536];size_t got;while((got=fread(b,1,sizeof b,f)))C(fwrite(b,1,got,o)==got);C(!ferror(f));fclose(f);}offsets[3840]=ftell(o);C(!fseek(o,16,SEEK_SET)&&fwrite(offsets,8,3841,o)==3841&&!fclose(o));unsigned char end;C(gzread(ids,&end,1)==0&&gzclose(ids)==Z_OK);puts("3840 final packets in frames.rsc; validate SHA256 before playback.");}
