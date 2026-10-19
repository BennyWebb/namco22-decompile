#!/usr/bin/env python3
"""Check captured bank/U/V/palette provenance directly against the user's ROMs."""
from pathlib import Path
import argparse,re,sys,struct,json
import numpy as np
p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('--roms',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);a=p.parse_args()
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assets'));from rom import ROM
rom=ROM(a.roms);data=(a.cache/'gles3.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',data);recipes=np.zeros((nt+512,7),np.int64)
for row in struct.iter_unpack('<8i',data[16+nm*12:]):recipes[row[7]]=row[:7]
dynamic={}
for m in re.finditer(r'GLES3-MATERIAL\] tick=(\d+) id=(\d+) recipe=([-0-9,]+)',(a.capture/'stderr.txt').read_text()):dynamic[int(m[1]),int(m[2])]=list(map(int,m[3].split(',')))
data=(a.capture/'palettes.bin').read_bytes();size=4+0x18000;assert len(data)==18*size;palettes={int.from_bytes(data[o:o+4],'little'):np.frombuffer(data,np.uint8,0x18000,o+4) for o in range(0,len(data),size)}
checks=[]
for f in sorted((a.capture/'images').glob('*.provenance.u32')):
 tick=int(f.name.split('.')[0].split('-')[1]);meta=np.fromfile(f,'<u4').reshape(480,640,4);covered=meta[:,:,3]>0;ids=meta[:,:,3][covered].astype(int)-1
 for mid in np.unique(ids):
  if mid>=nt:recipes[mid]=dynamic[tick,mid]
 r=recipes[ids];b,u,v=meta[:,:,:3][covered].T.astype(np.int64);assert np.array_equal(b,r[:,0]);ix=(((b*4096+v)&0xfff0)<<4)|((u&0xff0)>>4);tiles=rom.tiles[ix];attr=rom.attr[ix];x=u&15;y=v&15;x=np.where(attr&4,15-x,x);y=np.where(attr&2,15-y,y);xx=np.where(attr&8,y,x);yy=np.where(attr&8,x,y);raw=rom.cg[tiles*256+yy*16+xx].astype(np.uint32);c=r[:,2];shift=np.where(c&4,2*(~c&3),np.where(c&2,4*(~c&1),0));mask=np.where(c&4,3,np.where(c&2,15,255));add=np.where(c&4,0xec+((c&8)<<1),np.where(c&2,0xe0+((c&8)<<1),0));pen=((add+((raw>>shift)&mask))&255).astype(np.uint8);pal=palettes[tick];expected=np.stack([pal[r[:,1]*256+pen+k*32768] for k in range(3)],axis=1)
 image=np.fromfile(f.with_name(f'tick-{tick}.texel.rgba'),np.uint8).reshape(480,640,4);errors=int(np.count_nonzero(np.any(expected!=image[covered,:3],axis=1)));assert errors==0,(tick,errors);checks.append(dict(tick=tick,covered_pixels=int(covered.sum()),mismatches=errors))
assert len(checks)==18
result=dict(class_c=0,images=36,checks=checks);(a.capture/'ROM-proof.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS ROM provenance class(c)=0',sum(c['covered_pixels'] for c in checks),'pixels')
