#!/usr/bin/env python3
"""Verify frozen RGBA resources and emit persistent integer GPU source planes."""
import argparse,pathlib,numpy as np,json
p=argparse.ArgumentParser();p.add_argument('scene',type=pathlib.Path);p.add_argument('palette',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
romdir=pathlib.Path('raverace/extracted')
rom=np.frombuffer(b''.join((romdir/f'rv1cg{i}.{s}').read_bytes() for i,s in enumerate(['1a','1c','1d','1e','1f','1j','1k','1n'])),dtype=np.uint8)
tiles=np.frombuffer((romdir/'rv1ccrl.5a').read_bytes(),dtype='<u2').astype(np.uint32)
nib=np.frombuffer((romdir/'rv1ccrh.5c').read_bytes(),dtype=np.uint8);attr=np.empty(1048576,np.uint32);attr[::2]=nib>>4;attr[1::2]=nib&15
tiles|=(attr&1)<<16
pal=np.frombuffer(a.palette.read_bytes(),dtype=np.uint8).reshape(128,256,3)
errors=total=0;metadata=[]
for line in (a.scene/'resources.txt').read_text().splitlines():
 r,*k=map(int,line.split());w,h=k[1:3]
 with (a.scene/f'texture-{r}.rgba').open('rb') as f:
  f.readline();actual=np.frombuffer(f.read(),np.uint8).reshape(h,w,4)
 if w==h==1 and not any(k[3:]):
  expected=np.full((1,1,4),255,np.uint8);m=[w,h,1,1,0,0,0,0,0,0,255,3]
 else:
  su=max(1,(k[5]+k[11]-1)//k[11]);sv=max(1,(k[6]+k[11]-1)//k[11]);u=(k[3]+np.arange(w,dtype=np.uint32)*su)&4095;v=((k[4]+np.arange(h,dtype=np.uint32)*sv)&4095)|(k[7]*4096)
  idx=((v[:,None]&0xfff0)<<4)|((u[None,:]&0xff0)>>4);t=tiles[idx];at=attr[idx];x=np.broadcast_to(u[None,:]&15,(h,w));y=np.broadcast_to(v[:,None]&15,(h,w));x=np.where(at&4,15-x,x);y=np.where(at&2,15-y,y);xx=np.where(at&8,y,x);yy=np.where(at&8,x,y);off=t*256+yy*16+xx;raw=rom[np.minimum(off,len(rom)-1)];raw=np.where(off<len(rom),raw,0).astype(np.uint32)
  c=k[10]&15
  if c&4: offset=0xec+((c&8)<<1);shift=2*((~c)&3);mask=3
  elif c&2: offset=0xe0+((c&8)<<1);shift=4*((~c)&1);mask=15
  else:offset=shift=0;mask=255
  pen=(offset+((raw>>shift)&mask))&255;expected=np.empty((h,w,4),np.uint8);expected[:,:,:3]=pal[k[8]][pen];expected[:,:,3]=255
  m=[w,h,su,sv,k[3],k[4],k[7],k[8],offset,shift,mask,2]
 n=int(np.count_nonzero(np.any(actual!=expected,axis=2)));errors+=n;total+=w*h;metadata.append(m)
 if n:print('MISMATCH',r,n,k)
print('CPU_VERIFY',total,errors);assert errors==0,'Snapshot palette/source model does not reproduce frozen resources'
np.asarray(metadata,dtype='<u4').tofile(a.out/'metadata.bin')
rom.reshape(256,256,16,16).transpose(0,2,1,3).copy().tofile(a.out/'raw.bin')
(tiles|((attr&14)<<16)).astype('<u4').tofile(a.out/'raw-map.bin')
keys=np.where(tiles<65536,tiles*8+((attr&14)>>1),65536*8);unique,inverse=np.unique(keys,return_inverse=True);height=((len(unique)+255)//256)*16;out=np.zeros((height,4096),np.uint8);src=rom.reshape(65536,16,16)
for i,key in enumerate(unique):
 if key==65536*8:continue
 t=int(key)//8;at=(int(key)%8)<<1;image=src[t].T if at&8 else src[t]
 if at&2:image=image[::-1,:]
 if at&4:image=image[:,::-1]
 out[(i//256)*16:(i//256+1)*16,(i%256)*16:(i%256+1)*16]=image
out.tofile(a.out/'oriented.bin');inverse.astype('<u4').tofile(a.out/'oriented-map.bin')
rgba=np.empty((128,256,4),np.uint8);rgba[:,:,:3]=pal;rgba[:,:,3]=255;rgba.tofile(a.out/'palette.bin')
report=dict(resources=len(metadata),verified_texels=total,oriented_tiles=len(unique),oriented_height=height,raw_bytes=rom.size,map_bytes=tiles.size*4,oriented_bytes=out.size,palette_bytes=rgba.size)
(a.out/'sources.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
