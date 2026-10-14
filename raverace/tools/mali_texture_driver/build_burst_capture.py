"""Capture actual decoded payloads from fixed-history tick 1965, on the host.
No per-event filesystem writes until normal exit; not a performance run.
"""
from pathlib import Path
import subprocess,shlex
root=Path(__file__).resolve().parents[3];out=Path('/tmp/rr-mali-array/payload-capture');out.mkdir(parents=True,exist_ok=True);build=Path('/tmp/rr-playable-native-build')
s=(root/'engine/tex_bake.c').read_text()
hook=r'''
static struct {int w,h,bw,bh;unsigned char *data;} driver_payloads[512];
static int driver_payload_n;
static void driver_capture(int w,int h,int bw,int bh,const void *data){
 if(g_eng_frame!=1965)return;
 if(driver_payload_n>=512)abort();
 int i=driver_payload_n++;driver_payloads[i].w=w;driver_payloads[i].h=h;driver_payloads[i].bw=bw;driver_payloads[i].bh=bh;
 size_t bytes=(size_t)bw*bh*4;driver_payloads[i].data=malloc(bytes);if(!driver_payloads[i].data)abort();memcpy(driver_payloads[i].data,data,bytes);
}
__attribute__((destructor))static void driver_payload_dump(void){
 const char *dir=getenv("ENG_DRIVER_PAYLOAD_CAPTURE");if(!dir)return;
 char path[1024];snprintf(path,sizeof path,"%s/payloads.meta",dir);FILE *m=fopen(path,"wx");if(!m)abort();
 for(int i=0;i<driver_payload_n;i++){snprintf(path,sizeof path,"%s/payload-%d.rgba",dir,i);FILE *f=fopen(path,"wx");if(!f)abort();fprintf(m,"%d %d %d %d %d\n",i,driver_payloads[i].w,driver_payloads[i].h,driver_payloads[i].bw,driver_payloads[i].bh);fwrite(driver_payloads[i].data,4,(size_t)driver_payloads[i].bw*driver_payloads[i].bh,f);fclose(f);}fclose(m);
}
'''
request_hook=r'''
static struct {unsigned tag,a,b,c,d;} driver_requests[65536];static unsigned driver_request_n;
static void driver_request(unsigned tag,unsigned a,unsigned b,unsigned c,unsigned d){if(g_eng_frame!=1965)return;if(driver_request_n>=65536)abort();driver_requests[driver_request_n].tag=tag;driver_requests[driver_request_n].a=a;driver_requests[driver_request_n].b=b;driver_requests[driver_request_n].c=c;driver_requests[driver_request_n++].d=d;}
#undef tt_event
#define tt_event(tag,a,b,c,d) driver_request(tag,a,b,c,d)
__attribute__((destructor))static void driver_requests_dump(void){const char *dir=getenv("ENG_DRIVER_PAYLOAD_CAPTURE");if(!dir)return;char path[1024];snprintf(path,sizeof path,"%s/requests.csv",dir);FILE*f=fopen(path,"wx");if(!f)abort();fputs("tag,a,b,c,d\n",f);for(unsigned i=0;i<driver_request_n;i++)fprintf(f,"%u,%u,%u,%u,%u\n",driver_requests[i].tag,driver_requests[i].a,driver_requests[i].b,driver_requests[i].c,driver_requests[i].d);fclose(f);}
'''
s=s.replace('#include "gles_texture_trace.inc"','#include "gles_texture_trace.inc"\n'+request_hook)
(out/'tex_payload.c').write_text(s)
a=(root/'engine/gles_world_array_cache.inc').read_text();a=hook+a;a=a.replace('    world_reap(0);','    driver_capture(w,h,bw,bh,tex_pixel_buf);\n    world_reap(0);');(out/'gles_world_array_cache.inc').write_text(a)
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(name):return shlex.split(flags.split(name+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text())
subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-DRR_GLES_WORLD_ATLAS','-DRR_TEXTURE_TILE_RUNS','-c',str(out/'tex_payload.c'),'-o',str(out/'tex_payload.o')],check=True)
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/engine/tex_bake.c.o'):arg=str(out/'tex_payload.o')
 if arg=='rr_gles2':arg=str(out/'rr_payload_capture')
 cmd.append(arg)
subprocess.run(cmd,cwd=build,check=True)
