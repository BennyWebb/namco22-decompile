#!/usr/bin/env python3
"""Isolated M3 diagnostics: identical LUT, optional RG8 pen target/dead-depth discard."""
import argparse,pathlib,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);p.add_argument('--ticks',default='');a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[2]
subprocess.run([sys.executable,str(root/'native/tools/build_depth_lut_probe.py'),str(a.out),'--linear'],check=True)
s=(a.out/'probe.c').read_text();layer=(a.out/'layers.c').read_text();hud=(a.out/'rr_gpu_hud.inc').read_text();geo=(a.out/'rr_gpu_hud_geometry.inc').read_text()
s=s.replace('static int hud_control3;','static int native_rg4,native_invalidate4,native_color4;\nstatic int hud_control3;',1)
s=s.replace('native_front3=getenv(', 'native_color4=getenv("ENG_NATIVE_DISCARD_COLOR")&&!strcmp(getenv("ENG_NATIVE_DISCARD_COLOR"),"1");native_rg4=getenv("ENG_NATIVE_PEN_RG")&&!strcmp(getenv("ENG_NATIVE_PEN_RG"),"1");native_invalidate4=getenv("ENG_NATIVE_DISCARD_DEPTH")&&!strcmp(getenv("ENG_NATIVE_DISCARD_DEPTH"),"1");native_front3=getenv(',1)
s=s.replace(' double load_start=', ' printf("TAIL_OPTIONS pen_rg=%d discard_depth=%d\\n",native_rg4,native_invalidate4);\n double load_start=',1)
s=s.replace('double world_submit=clock3(CLOCK_MONOTONIC);', 'if(native_invalidate4){const GLenum dead4=GL_DEPTH_ATTACHMENT;glInvalidateFramebuffer(GL_FRAMEBUFFER,1,&dead4);}double world_submit=clock3(CLOCK_MONOTONIC);')
if a.ticks:
 old='getenv("ENG_NATIVE_SEQUENCE")||h->tick==1800||h->tick==1965||h->tick==2500||h->tick==2643||h->tick==3000||h->tick==3546||h->tick==3600||h->tick==4200||h->tick==4800||h->tick==5400||h->tick==5639'
 ticks=[int(v) for v in a.ticks.split(',')];assert all(1800<=t<=5639 for t in ticks);assert old in s;s=s.replace(old,'getenv("ENG_NATIVE_SEQUENCE")||'+'||'.join('h->tick=='+str(t) for t in sorted(set(ticks))))
hud=hud.replace('glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);', 'glBindFramebuffer(GL_FRAMEBUFFER,0);if(native_color4){const GLenum dead4=GL_COLOR;glInvalidateFramebuffer(GL_FRAMEBUFFER,1,&dead4);}glViewport(0,0,W,H);')
s=s.replace('TAIL_OPTIONS pen_rg=%d discard_depth=%d', 'TAIL_OPTIONS pen_rg=%d discard_depth=%d discard_color=%d').replace('native_rg4,native_invalidate4);', 'native_rg4,native_invalidate4,native_color4);')
# Only the pen target uses RG8. Red is original pen index; green stores original coverage.
hud=hud.replace('raw_tex=gpu_geometry()?texture(W,H):texture(256,128);', '''raw_tex=gpu_geometry()?texture(W,H):texture(256,128);
 if(native_rg4){glBindTexture(GL_TEXTURE_2D,raw_tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RG8,W,H,0,GL_RG,GL_UNSIGNED_BYTE,NULL);}''')
hud=hud.replace('if(gpu_geometry())fs=native_fold3?native_fold_fs3:glyph_final_fs;', '''if(gpu_geometry())fs=native_fold3?native_fold_fs3:glyph_final_fs;
 char *rg_fs4=NULL;if(native_rg4){if(native_fold3)return false;rg_fs4=strdup(fs);char *coverage4=strstr(rg_fs4,"t.a>.5");if(!coverage4){free(rg_fs4);return false;}coverage4[2]='g';fs=rg_fs4;}''')
hud=hud.replace('GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);','GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);free(rg_fs4);',1)
# Green is irrelevant to RGBA baseline's final shader; RG stores it as coverage.
geo=geo.replace('gl_FragColor=vec4((floor(pal+.5)*16.+p)/255.,0.,0.,1.);','gl_FragColor=vec4((floor(pal+.5)*16.+p)/255.,1.,0.,1.);')
for name,data in [('probe.c',s),('layers.c',layer),('rr_gpu_hud.inc',hud),('rr_gpu_hud_geometry.inc',geo)]: (a.out/name).write_text(data)
