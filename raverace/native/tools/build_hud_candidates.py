#!/usr/bin/env python3
"""Isolated native HUD candidates with independent cull/UBO flags and fallbacks."""
import argparse,pathlib,subprocess,sys,json
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[2]
subprocess.run([sys.executable,str(root/'native/tools/build_hud_probe.py'),str(a.out)],check=True)
probe=(a.out/'probe.c').read_text();geo=(a.out/'rr_gpu_hud_geometry.inc').read_text();hud=(a.out/'rr_gpu_hud.inc').read_text()
probe=probe.replace('static int hud_control3;','static int hud_cull3,hud_ubo3;static GLuint hud_lut_buffer3;\nstatic int hud_control3;')
probe=probe.replace('hud_control3=getenv(', 'hud_cull3=getenv("ENG_NATIVE_HUD_CULL")&&!strcmp(getenv("ENG_NATIVE_HUD_CULL"),"1");hud_ubo3=getenv("ENG_NATIVE_HUD_UBO")&&!strcmp(getenv("ENG_NATIVE_HUD_UBO"),"1");hud_control3=getenv(',1)
probe=probe.replace('hud_control3<=2','hud_control3<=3&&(!hud_ubo3||hud_control3==0)')
probe=probe.replace('double load_start=clock3(CLOCK_MONOTONIC);','printf("HUDOPTIONS cull=%d ubo=%d ubo_bytes=%u\\n",hud_cull3,hud_ubo3,hud_ubo3?4096:0);\n double load_start=clock3(CLOCK_MONOTONIC);')
probe=probe.replace('shadow_enabled=%u\\n",h->tick','shadow_enabled=%u glyph_vertices=%d\\n",h->tick').replace('hud_pen_cpu3,hs[3]);','hud_pen_cpu3,hs[3],glyph_vertex_count);')
# Probe-only alternate font fixture: alternate a currently blank glyph without tilemap edits.
probe=probe.replace('g_eng_frame=h->tick;CHECK3(rr_video_layers_compose_gpu(&hud));','if(getenv("ENG_NATIVE_HUD_FONT_FIXTURE")){static uint8_t fixture_font3[FONT3];memcpy(fixture_font3,hud.characters,FONT3);if(h->tick&1)memset(fixture_font3+32*128,0,128);hud.characters=fixture_font3;}g_eng_frame=h->tick;CHECK3(rr_video_layers_compose_gpu(&hud));')
geo='static bool native_blank3[1024];\n'+geo
mark=' bool rebuild='
code=''' bool blank3[1024]={0};bool blank_changed3=false;
 if(hud_cull3){uint8_t transparent3[128];memset(transparent3,255,sizeof transparent3);for(unsigned code3=0;code3<1024;code3++){const uint8_t*data3=code3<960?frame->characters+code3*128:frame->tilemap+(code3-960)*128;blank3[code3]=!memcmp(data3,transparent3,128);blank_changed3|=blank3[code3]!=native_blank3[code3];}memcpy(native_blank3,blank3,sizeof blank3);}
 bool rebuild=blank_changed3||'''
assert mark in geo;geo=geo.replace(mark,code,1)
mark='code=word&1023;needed_glyphs[code]=1;';assert mark in geo;geo=geo.replace(mark,'code=word&1023;if(hud_cull3&&blank3[code])continue;needed_glyphs[code]=1;')
# Refine shadow attribution: keep shadow selection, bypass only arithmetic.
start=geo.index('static const char *glyph_final_fs=');end=geo.index('static const char *glyph_final_no_lut_fs=',start);block=geo[start:end];shadow=block.replace('glyph_final_fs','glyph_final_no_shadow_math_fs').replace('c=floor(c*m/256.);','c=c;');geo=geo[:end]+shadow+geo[end:]
ubo=(root/'native/render/hud_lut_ubo.frag').read_text();geo='static const char *native_ubo_fs3='+json.dumps(ubo)+';\n'+geo
hud=hud.replace('hud_control3==2?glyph_final_no_lut_fs:glyph_final_fs','hud_control3==2?glyph_final_no_lut_fs:hud_control3==3?glyph_final_no_shadow_math_fs:glyph_final_fs')
mark=' GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);'
code=''' if(hud_ubo3){if(!gpu_geometry())return false;vs="#version 300 es\\nin vec2 a_position;out highp vec2 uv;void main(){gl_Position=vec4(a_position,0.,1.);uv=a_position*.5+.5;}\\n";fs=native_ubo_fs3;}
'''+mark
assert mark in hud;hud=hud.replace(mark,code,1)
mark=' const char *names[]={"scene_tex","raw_tex","palette_tex","lut_tex"};'
code=''' if(hud_ubo3){GLint cap3,blocks3;glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,&cap3);glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,&blocks3);if(cap3<4096||blocks3<1)return false;GLuint block3=glGetUniformBlockIndex(gpu_program,"NativeLUT");if(block3==GL_INVALID_INDEX)return false;glUniformBlockBinding(gpu_program,block3,0);glGenBuffers(1,&hud_lut_buffer3);glBindBuffer(GL_UNIFORM_BUFFER,hud_lut_buffer3);glBufferData(GL_UNIFORM_BUFFER,4096,NULL,GL_DYNAMIC_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,0,hud_lut_buffer3);}
'''+mark
assert mark in hud;hud=hud.replace(mark,code,1)
mark='  glBindTexture(GL_TEXTURE_2D,lut_tex);glTexSubImage2D'
code='''  if(hud_ubo3){float values3[256][4];for(unsigned i3=0;i3<256;i3++)for(unsigned c3=0;c3<4;c3++)values3[i3][c3]=frame->lut[i3][c3]/255.f;glBindBuffer(GL_UNIFORM_BUFFER,hud_lut_buffer3);glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof values3,values3);gpu_account(sizeof values3);}
'''+mark
assert mark in hud;hud=hud.replace(mark,code,1)
hud=hud.replace(' geometry_close();GLuint ts[]=', ' if(hud_lut_buffer3)glDeleteBuffers(1,&hud_lut_buffer3);\n geometry_close();GLuint ts[]=')
for name,data in [('probe.c',probe),('rr_gpu_hud.inc',hud),('rr_gpu_hud_geometry.inc',geo)]: (a.out/name).write_text(data)
