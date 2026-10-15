#!/usr/bin/env python3
"""GL fixture changes palette/mix/shadow independently with a frozen LUT/text."""
import argparse,pathlib,subprocess,os,json
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('shaders',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(exist_ok=False);root=pathlib.Path(__file__).resolve().parents[3]
s=a.source.read_text();mark='memcpy(hud.lut,style+1049,1024);';assert mark in s
s=s.replace(mark,mark+'''static uint8_t frozen_lut3[256][4],frozen_text3[8192];static int fixture_valid3;if(!fixture_valid3){memcpy(frozen_lut3,hud.lut,1024);memcpy(frozen_text3,hud.tilemap,8192);fixture_valid3=1;}memcpy(hud.lut,frozen_lut3,1024);hud.tilemap=frozen_text3;unsigned phase3=h->tick%4;hud.shadow_enabled=phase3!=2;for(unsigned i3=0;i3<256;i3++)for(unsigned c3=0;c3<3;c3++)hud.palette[i3][c3]=(uint8_t)(i3*(c3+1)+phase3*19);for(unsigned i3=0;i3<3;i3++)for(unsigned c3=0;c3<3;c3++)hud.multipliers[i3][c3]=(uint8_t)((phase3+i3+c3)%3==0?0:(phase3+i3+c3)%3==1?128:255);''')
source=a.out/'fixture.c';source.write_text(s);exe=a.out/'fixture';flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl2','glesv2'],text=True).split();subprocess.run(['cc','-O2','-Wall','-Wextra','-DRR_GPU_HUD','-I'+str(root/'raverace/include'),'-I'+str(root/'engine'),str(source),'-o',str(exe),*flags,'-lm','-lz','-pthread'],check=True)
for fold in [0,1]:
 out=a.out/str(fold);out.mkdir();env=dict(os.environ,SDL_VIDEODRIVER='offscreen',ENG_NATIVE_SEQUENCE='1',ENG_NATIVE_FRONT='0',ENG_NATIVE_FOLD=str(fold),ENG_NATIVE_LOG='ram')
 with (out/'run.txt').open('w') as log:subprocess.run([str(exe),str(a.pack),str(a.shaders),'2500','2505','1',str(out)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
for tick in range(2500,2506):assert (a.out/'0'/f'tick-{tick}.composed.rgba').read_bytes()==(a.out/'1'/f'tick-{tick}.composed.rgba').read_bytes(),tick
report=dict(frames=6,frozen_lut_and_tilemap=True,changing_palette_mix_and_shadow=True,multiplier_values=[0,128,255],all_composed_bytes_match=True);(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
