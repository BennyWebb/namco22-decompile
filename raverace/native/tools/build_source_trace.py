#!/usr/bin/env python3
"""Privately relink headless diagnostics; leave shared source/build objects intact."""
import argparse,json,pathlib,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];build=a.build.resolve();out=a.out.resolve()
assert not out.exists(),'use a new output directory'
assert not out.is_relative_to(root),'diagnostic outputs must be outside repository'
out.mkdir(parents=True);capture=out/'capture';capture.mkdir()
flags=(build/'CMakeFiles/rr_headless.dir/flags.make').read_text()
def get(n):return shlex.split(flags.split(n+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_headless.dir/link.txt').read_text())
source=(root/'raverace/gen/rr_c25.c').read_text();needle='        d->ops = (O); d->pc = (uint16_t)((PC) + 1);'
assert source.count(needle)==1
source=source.replace(needle,'        if ((OP)==0xCC45 && d->pdp_begins>=2390) fprintf(stderr,"[M1-MAP] pdp=%u pc=%04x input=%d output=%d\\n",d->pdp_begins,(PC),d->acc,d->acc+69); \\\n'+needle)
(out/'dsp_trace.c').write_text(source)
s=r'''
#include <stdio.h>
#include <stdlib.h>
#include "rr_mem.h"
#include "rr_dsp.h"
static void save(const char *name,const void *p,size_t n){char path[4096];snprintf(path,sizeof path,"%s/%s",CAPTURE,name);FILE*f=fopen(path,"wbx");if(!f)abort();if(fwrite(p,1,n,f)!=n)abort();fclose(f);}
__attribute__((destructor))static void capture(void){save("poly.i32",g_rr.poly,sizeof g_rr.poly);save("wram.bin",g_rr.wram,sizeof g_rr.wram);save("program.bin",g_rr.rom,sizeof g_rr.rom);int32_t*p=malloc(0x20000*4);if(!p)abort();for(unsigned i=0;i<0x20000;i++)p[i]=rr_dsp_pointram_read(0xf00000+i);save("pointram.i32",p,0x20000*4);free(p);}
'''
(out/'capture.c').write_text('#define CAPTURE '+json.dumps(str(capture))+'\n'+s)
for name in ['dsp_trace','capture']:
    subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-O2','-w','-c',str(out/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
cmd=[]
for x in link:
    if x.startswith('-Wl,--dependency-file'):continue
    if x.endswith('/gen/rr_c25.c.o'):x=str(out/'dsp_trace.o')
    if x=='rr_headless':x=str(out/'rr_asset_trace')
    cmd.append(x)
cmd.append(str(out/'capture.o'));subprocess.run(cmd,cwd=build,check=True)
print(out/'rr_asset_trace');print('Run once through tick 2500; capture files use exclusive creation.')
