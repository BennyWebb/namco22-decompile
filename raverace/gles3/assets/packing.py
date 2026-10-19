import math
def contains(a,b):return a[0]<=b[0] and a[1]<=b[1] and a[0]+a[2]>=b[0]+b[2] and a[1]+a[3]>=b[1]+b[3]
def pack(items,extent):
    pages=[];placements={}
    for key,w,h in sorted(items,key=lambda x:(-max(x[1:]),-x[1]*x[2],x[0])):
        if w>extent or h>extent:raise ValueError(f'material {key} exceeds page extent: {w}x{h}')
        fits=[]
        for page,free in enumerate(pages):
            for r in free:
                if w<=r[2] and h<=r[3]:fits.append(((min(r[2]-w,r[3]-h),max(r[2]-w,r[3]-h),page,r[1],r[0]),page,r))
        if not fits:
            pages.append([(0,0,extent,extent)]);page=len(pages)-1;r=pages[page][0]
        else:_,page,r=min(fits)
        x,y=r[:2];used=(x,y,w,h);new=[]
        for fx,fy,fw,fh in pages[page]:
            if x>=fx+fw or x+w<=fx or y>=fy+fh or y+h<=fy:new.append((fx,fy,fw,fh));continue
            if x>fx:new.append((fx,fy,x-fx,fh))
            if x+w<fx+fw:new.append((x+w,fy,fx+fw-x-w,fh))
            if y>fy:new.append((fx,fy,fw,y-fy))
            if y+h<fy+fh:new.append((fx,y+h,fw,fy+fh-y-h))
        unique=sorted(set(new));pages[page]=[r for i,r in enumerate(unique) if not any(i!=j and contains(other,r) for j,other in enumerate(unique))]
        placements[key]=(page,x,y,w,h)
    dims=[]
    for page in range(len(pages)):
        rs=[r for r in placements.values() if r[0]==page]
        # 4-texel alignment; NPOT clamp/nearest baseline, mipmaps remain optional.
        dims.append((math.ceil(max(r[1]+r[3] for r in rs)/4)*4,math.ceil(max(r[2]+r[4] for r in rs)/4)*4))
    return placements,dims

