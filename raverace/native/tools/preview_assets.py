#!/usr/bin/env python3
"""Small offline asset preview, using orthographic CPU rasterisation only."""
import argparse,base64,json,pathlib,struct,zlib
import numpy as np
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--models',default='1280,1945');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
def png(path,pixels):
    h,w,_=pixels.shape
    def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b)&0xffffffff)
    data=b''.join(b'\0'+row.tobytes() for row in pixels)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(data))+chunk(b'IEND',b''))
mat=json.loads((a.pack/'materials.json').read_bytes());textures={}
for i,r in enumerate(mat['baked']):
    bank,pal,c,u,v,w,h=r;f=a.pack/f'material-{i}.rgba'
    if f.exists():
        image=np.frombuffer(f.read_bytes(),np.uint8).reshape(h,w,4)[:,:,:3];textures[tuple(r)]=image
        png(a.out/f'texture-{i}.png',image)
for code in map(int,a.models.split(',')):
    m=json.loads((a.pack/f'model-{code}.json').read_bytes());vertices=np.array(m['vertices'],dtype=float);xyz=vertices[:,:3]
    if code==1765:screen=xyz[:,[2,1]]*np.array([1,-1]);depth=xyz[:,0]
    else:
        yaw=.22 if code==1280 else .65;c,s=np.cos(yaw),np.sin(yaw);x=c*xyz[:,0]-s*xyz[:,2];z=s*xyz[:,0]+c*xyz[:,2]
        screen=np.column_stack([x,.82*z-.57*xyz[:,1]]);depth=.57*z+.82*xyz[:,1]
    lo=screen.min(0);hi=screen.max(0);size=900;scale=(size-80)/max(hi-lo);screen=(screen-(lo+hi)/2)*scale+size/2
    img=np.zeros((size,size,3),np.uint8);img[:]=[22,28,38]
    zbuf=np.full((size,size),-np.inf)
    for q in m['quads']:
        n=q['first_vertex'];v=vertices[n:n+4];uu=v[:,3];vv=v[:,4]
        recipe=tuple(q['material'])+(int(uu.min()),int(vv.min()),int(uu.max()-uu.min()+1),int(vv.max()-vv.min()+1))
        tex=textures.get(recipe)
        if tex is None:continue
        for offsets in [(0,1,2),(0,2,3)]:
            ids=np.array(offsets)+n;pts=screen[ids];xs=pts[:,0];ys=pts[:,1]
            x0=max(0,int(np.floor(xs.min())));x1=min(size-1,int(np.ceil(xs.max())));y0=max(0,int(np.floor(ys.min())));y1=min(size-1,int(np.ceil(ys.max())))
            if x1<x0 or y1<y0:continue
            X,Y=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
            den=(ys[1]-ys[2])*(xs[0]-xs[2])+(xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(den)<1e-8:continue
            b0=((ys[1]-ys[2])*(X-xs[2])+(xs[2]-xs[1])*(Y-ys[2]))/den
            b1=((ys[2]-ys[0])*(X-xs[2])+(xs[0]-xs[2])*(Y-ys[2]))/den;b2=1-b0-b1
            ds=b0*depth[ids[0]]+b1*depth[ids[1]]+b2*depth[ids[2]];Z=zbuf[y0:y1+1,x0:x1+1]
            mask=(b0>=-1e-8)&(b1>=-1e-8)&(b2>=-1e-8)&(ds>=Z)
            U=b0*vertices[ids[0],3]+b1*vertices[ids[1],3]+b2*vertices[ids[2],3]-recipe[3]
            V=b0*vertices[ids[0],4]+b1*vertices[ids[1],4]+b2*vertices[ids[2],4]-recipe[4]
            ix=np.clip(np.floor(U+.5).astype(int),0,tex.shape[1]-1);iy=np.clip(np.floor(V+.5).astype(int),0,tex.shape[0]-1)
            region=img[y0:y1+1,x0:x1+1];region[mask]=tex[iy[mask],ix[mask]];Z[mask]=ds[mask]
    png(a.out/f'model-{code}.png',img)
# Contact sheet uses nearest scaling, preserves each image's aspect ratio.
items=sorted(textures.items(),key=lambda kv:mat['baked'].index(list(kv[0])))
cols=6;cellw,cellh=190,150;rows=(len(items)+cols-1)//cols;sheet=np.zeros((rows*cellh,cols*cellw,3),np.uint8);sheet[:]=[22,28,38]
for i,(_,tex) in enumerate(items):
    h,w=tex.shape[:2];sc=min((cellw-16)/w,(cellh-16)/h);nw=max(1,int(w*sc));nh=max(1,int(h*sc));sample=tex[(np.arange(nh)*h//nh)[:,None],(np.arange(nw)*w//nw)[None,:]]
    x=i%cols*cellw+(cellw-nw)//2;y=i//cols*cellh+(cellh-nh)//2;sheet[y:y+nh,x:x+nw]=sample
png(a.out/'textures.png',sheet)
print(a.out);print(f'{len(items)} baked textures, selected extracted meshes; diagnostic views without game lighting/fog.')
