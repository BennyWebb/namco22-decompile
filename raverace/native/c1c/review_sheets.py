#!/usr/bin/env python3
import pathlib,struct,zlib,numpy as np
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1c';old=root/'raverace/native/generated/c1b';ps=sorted((old/'host-final-rgba/images').glob('*.rgba'))
def png(p,a):
 h,w,_=a.shape
 def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
 p.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+r.tobytes() for r in a)))+chunk(b'IEND',b''))
for k in range(6):
 tiles=[]
 for p in ps[k*6:(k+1)*6]:
  a=np.fromfile(p,np.uint8).reshape(480,640,4);b=np.fromfile(base/'texture-lowp/images'/p.name,np.uint8).reshape(480,640,4);m=np.any(a!=b,axis=2);heat=np.zeros_like(a[:,:,:3]);heat[m]=[255,0,255];tiles.append(np.concatenate([a[:,:,:3],b[:,:,:3],heat],axis=1)[::-2,::2])
 png(base/'review'/f'contact-{k+1}.png',np.concatenate(tiles,axis=0))
