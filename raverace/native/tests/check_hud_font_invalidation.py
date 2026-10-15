#!/usr/bin/env python3
"""Actual GL fixture: freeze text RAM, change a blank glyph, compare cull on/off."""
import argparse,pathlib,subprocess,os,json,re
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('shaders',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(exist_ok=False);root=pathlib.Path(__file__).resolve().parents[3]
s=a.source.read_text();old='if(h->tick&1)memset(fixture_font3+32*128,0,128);hud.characters=fixture_font3;';new='static uint8_t fixture_tile3[8192];static int fixture_valid3;if(!fixture_valid3){memcpy(fixture_tile3,hud.tilemap,8192);fixture_valid3=1;}hud.tilemap=fixture_tile3;unsigned phase3=h->tick%4;memset(fixture_font3+32*128,phase3==1?0:phase3==2?0x11:0xff,128);hud.characters=fixture_font3;';assert old in s;s=s.replace(old,new);source=a.out/'fixture.c';source.write_text(s);exe=a.out/'fixture'
flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl2','glesv2'],text=True).split();subprocess.run(['cc','-O2','-Wall','-Wextra','-DRR_GPU_HUD','-I'+str(root/'raverace/include'),'-I'+str(root/'engine'),str(source),'-o',str(exe),*flags,'-lm','-lz','-pthread'],check=True)
for cull in [0,1]:
 out=a.out/str(cull);out.mkdir();env=dict(os.environ,SDL_VIDEODRIVER='offscreen',ENG_NATIVE_SEQUENCE='1',ENG_NATIVE_HUD_FONT_FIXTURE='1',ENG_NATIVE_HUD_CULL=str(cull),ENG_NATIVE_HUD_UBO='1')
 with (out/'run.txt').open('w') as log:subprocess.run([str(exe),str(a.pack),str(a.shaders),'2500','2505','1',str(out)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
for tick in range(2500,2506):assert (a.out/'0'/f'tick-{tick}.composed.rgba').read_bytes()==(a.out/'1'/f'tick-{tick}.composed.rgba').read_bytes(),tick
log=(a.out/'1/run.txt').read_text();counts=[int(x) for x in re.findall(r'HUDPROFILE tick=\d+.*glyph_vertices=(\d+)',log)][-6:];assert counts[0]<counts[1] and counts[1]==counts[2] and counts[2]>counts[3],counts
report=dict(frames=6,frozen_tilemap=True,glyph32_phases=['transparent','pen0','pen1','transparent','transparent','pen0'],cull_vertex_counts=counts,all_composed_bytes_match=True);(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
