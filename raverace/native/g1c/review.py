#!/usr/bin/env python3
import pathlib,numpy as np,struct,zlib
b=pathlib.Path(__file__).resolve().parents[1]/'generated/g1c';out=b/'mali-review';out.mkdir(exist_ok=True)
def png(p,a):
 h,w,_=a.shape
 def c(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 p.write_bytes(b'\x89PNG\r\n\x1a\n'+c(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+c(b'IDAT',zlib.compress(b''.join(b'\0'+r.tobytes() for r in a)))+c(b'IEND',b''))
ps=sorted((pathlib.Path(__file__).resolve().parents[1]/'generated/c1e/device-colour-rgba/images').glob('*.rgba'));assert len(ps)==36
for p in ps:
 a=np.fromfile(p,np.uint8).reshape(480,640,4);d=np.fromfile(b/'device/colour-index/images'/p.name,np.uint8).reshape(480,640,4);m=np.any(a!=d,axis=2);heat=a[:,:,:3].copy();heat[m]=[255,0,255];png(out/(p.name+'.png'),np.concatenate([a[:,:,:3],d[:,:,:3],heat],axis=1)[::-1])

small=[]
for p in ps:
 a=np.fromfile(p,np.uint8).reshape(480,640,4);d=np.fromfile(b/'device/colour-index/images'/p.name,np.uint8).reshape(480,640,4);heat=a[:,:,:3].copy();heat[np.any(a!=d,axis=2)]=[255,0,255];small.append(np.concatenate([a[:,:,:3],d[:,:,:3],heat],axis=1)[::-4,::4])
for i in range(6):png(out/f'contact-{i}.png',np.concatenate([np.concatenate(small[i*6:i*6+3],axis=1),np.concatenate(small[i*6+3:i*6+6],axis=1)],axis=0))
