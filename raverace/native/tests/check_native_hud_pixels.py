#!/usr/bin/env python3
"""Independent byte compositor oracle for actual native world/composed readbacks."""
import argparse,pathlib,sys,struct,json,numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'));from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('captures',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();reader=Reader(a.pack/'frames.rsc');fonts=(a.pack/'hud-fonts.bin').read_bytes();styles=(a.pack/'hud-styles.bin').read_bytes();yy,xx=np.indices((480,640));rows=[]
for file in sorted(a.captures.glob('tick-*.world.rgba')):
 tick=int(file.name.split('-')[1].split('.')[0]);f=reader.read(tick);h=f['header'];style=styles[h[9]*2073:(h[9]+1)*2073];_,sx,sy,shadow=struct.unpack_from('<4I',style);font=fonts[h[8]*122880:(h[8]+1)*122880];raw=np.frombuffer(font+f['text'],np.uint8);tile=np.frombuffer(f['text'],np.uint8);palette=np.frombuffer(style[16:1040],np.uint8).reshape(256,4);mix=np.frombuffer(style[1040:1049],np.uint8).reshape(3,3);lut=np.frombuffer(style[1049:],np.uint8).reshape(256,4)
 px=(xx+sx)&1023;py=(yy+sy)&1023;off=((py//16)*64+px//16)*2;word=tile[off].astype(np.uint16)*256+tile[off+1];qx=np.where(word&1024,15-(px&15),px&15);qy=np.where(word&2048,15-(py&15),py&15);value=raw[(word&1023).astype(np.int64)*128+qy*8+qx//2];pen=np.where(qx&1,value&15,value>>4);idx=(word>>12)*16+pen
 world=np.flipud(np.fromfile(file,np.uint8).reshape(480,640,4));actual=np.flipud(np.fromfile(a.captures/f'tick-{tick}.composed.rgba',np.uint8).reshape(480,640,4));expected=world.copy();visible=(pen<15)&(world[:,:,3]<128);shade=visible&(idx>=252)&(idx<=254)&bool(shadow);normal=visible&~shade;expected[normal,:3]=palette[idx[normal],:3];expected[shade,:3]=(expected[shade,:3].astype(np.uint32)*mix[idx[shade]-252].astype(np.uint32)//256).astype(np.uint8)
 for c in range(3):expected[:,:,c]=lut[expected[:,:,c],c]
 expected[:,:,3]=255;delta=np.abs(expected.astype(int)-actual.astype(int));rows.append(dict(tick=tick,visible_hud_pixels=int(visible.sum()),visible_shadow_pixels=int(shade.sum()),changed_pixels=int(np.any(delta,axis=2).sum()),changed_channels=int(np.count_nonzero(delta)),maximum=int(delta.max())))
assert rows
out=dict(scope='Original byte palette/shadow/LUT CPU oracle from the actual native world readback and exact source HUD bytes.',frames=len(rows),all_pixels_match=all(r['changed_pixels']==0 for r in rows),samples=rows);a.out.write_text(json.dumps(out,indent=2)+'\n');print(out);assert out['all_pixels_match']
