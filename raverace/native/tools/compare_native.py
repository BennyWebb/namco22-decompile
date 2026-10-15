#!/usr/bin/env python3
"""Compare two bottom-up 640x480 raw RGBA captures and make labelled-side inputs."""
import argparse,json,pathlib,struct,zlib
import numpy as np
p=argparse.ArgumentParser();p.add_argument('reference',type=pathlib.Path);p.add_argument('candidate',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
x=np.frombuffer(a.reference.read_bytes(),np.uint8).reshape(480,640,4)[::-1];y=np.frombuffer(a.candidate.read_bytes(),np.uint8).reshape(480,640,4)[::-1];err=np.abs(x.astype(np.int16)-y);mask=np.any(err,axis=2);rgbmask=np.any(err[:,:,:3],axis=2)
def png(name,data):
 data=np.ascontiguousarray(data);h,w,_=data.shape
 def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b)&0xffffffff)
 payload=b''.join(b'\0'+row.tobytes() for row in data)
 (a.out/name).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(payload))+chunk(b'IEND',b''))
png('reference.png',x[:,:,:3]);png('native.png',y[:,:,:3]);png('side-by-side.png',np.concatenate([x[:,:,:3],y[:,:,:3]],axis=1));heat=np.zeros((480,640,3),np.uint8);heat[rgbmask]=[255,70,10];png('difference.png',heat)
ys,xs=np.where(rgbmask)
if len(xs):
 left=max(0,int(xs.min())-8);right=min(640,int(xs.max())+9);top=max(0,int(ys.min())-8);bottom=min(480,int(ys.max())+9)
 # Enlarge the most concentrated changed-pixel tile, rather than the full view.
 bins=[(int(rgbmask[i:i+48,j:j+64].sum()),i,j) for i in range(0,480,48) for j in range(0,640,64)];_,top,left=max(bins);crop=np.concatenate([x[top:top+48,left:left+64,:3],y[top:top+48,left:left+64,:3]],axis=1);png('enlarged.png',np.repeat(np.repeat(crop,6,axis=0),6,axis=1))
report=dict(changed_rgba_pixels=int(mask.sum()),changed_rgb_pixels=int(rgbmask.sum()),changed_alpha_pixels=int(np.count_nonzero(err[:,:,3])),maximum_channel_error=int(err.max()),mean_rgb_absolute_error=float(err[:,:,:3].mean()),changed_pixel_fraction=float(mask.mean()))
(a.out/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
