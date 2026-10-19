#!/usr/bin/env python3
"""ROM-only footprint index atlas. Reference-pack inputs are solely parity/sizing."""
import argparse,collections,hashlib,heapq,json,pathlib,struct,sys,time,zipfile
import numpy as np
from rom import ROM,validate_model
from packing import pack

def digest(b):return hashlib.sha256(b).hexdigest()
def encode(x):return (json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
def merge_rects(rects):
 # Best area saving first, deterministic ties. Only strictly overlapping rectangles
 # of the same bank/cmode; union bounds must save unpadded AND padded area.
 n=len(rects);capacity=max(1,2*n);arr=np.zeros((capacity,6),np.int64);arr[:n]=rects;active=np.zeros(capacity,bool);active[:n]=True
 parent=list(range(capacity));heap=[];savings=[]
 def candidates(i):
  r=arr[i];ids=np.flatnonzero(active[:i]);s=arr[ids]
  if not len(ids):return
  x=np.maximum(r[2],s[:,2]);y=np.maximum(r[3],s[:,3]);right=np.minimum(r[2]+r[4],s[:,2]+s[:,4]);bottom=np.minimum(r[3]+r[5],s[:,3]+s[:,5]);ok=(r[0]==s[:,0])&(r[1]==s[:,1])&(right>x)&(bottom>y)
  for j in ids[ok]:
   t=arr[j];u=min(r[2],t[2]);v=min(r[3],t[3]);w=max(r[2]+r[4],t[2]+t[4])-u;h=max(r[3]+r[5],t[3]+t[5])-v
   save=int((r[4]+4)*(r[5]+4)+(t[4]+4)*(t[5]+4)-(w+4)*(h+4))
   if w<=4092 and h<=4092 and w*h<r[4]*r[5]+t[4]*t[5] and save>0:heapq.heappush(heap,(-save,int(j),i,(int(r[0]),int(r[1]),int(u),int(v),int(w),int(h))))
 for i in range(n):candidates(i)
 while heap:
  minus,a,b,r=heapq.heappop(heap)
  if not active[a] or not active[b]:continue
  active[a]=active[b]=False;parent[a]=parent[b]=n;arr[n]=r;active[n]=True;savings.append(-minus);candidates(n);n+=1
 def root(i):
  while parent[i]!=i:i=parent[i]
  return i
 return {int(i):tuple(map(int,arr[i])) for i in np.flatnonzero(active[:n])},[root(i) for i in range(len(rects))],savings

def build(rom,recipes,out,label):
 start=time.monotonic();out.mkdir(parents=True,exist_ok=True);initial={};reps=[];aliases={};bytecount=0;exact_hits=0
 for r in sorted(set(recipes)):
  b,c,u,v,w,h=r;assert 0<w<=4092 and 0<h<=4092,r
  image=rom.pens(b,c,(u-2)&4095,(v-2)&4095,w+4,h+4);blob=image.tobytes();key=digest(struct.pack('<II',w+4,h+4)+blob)
  if key in initial:
   assert initial[key][1]==blob,'Digest collision';index=initial[key][0];exact_hits+=1
  else:index=len(reps);reps.append(r);initial[key]=(index,blob);bytecount+=len(blob)
  aliases[r]=index
 print(label,'decoded',len(recipes),'unique',len(reps),'MiB',bytecount/2**20,flush=True)
 roots,parents,savings=merge_rects(reps);print(label,'merged',len(roots),'savedMiB',sum(savings)/2**20,flush=True)
 # Free initial data before constructing final atlas images; dedup is exact again.
 initial.clear();contents={};rootkeys={};sizes={}
 for i,r in sorted(roots.items()):
  b,c,u,v,w,h=r;blob=rom.pens(b,c,(u-2)&4095,(v-2)&4095,w+4,h+4).tobytes();key=digest(struct.pack('<II',w+4,h+4)+blob)
  if key in contents:assert contents[key]==blob,'Digest collision'
  else:contents[key]=blob;sizes[key]=(w+4,h+4)
  rootkeys[i]=key
 placements,dims=pack([(k,*sizes[k]) for k in sorted(contents)],4096)
 pages=[np.zeros((h,w),np.uint8) for w,h in dims]
 for k,blob in contents.items():
  p,x,y,w,h=placements[k];pages[p][y:y+h,x:x+w]=np.frombuffer(blob,np.uint8).reshape(h,w)
 mappings=[]
 for r,i in sorted(aliases.items()):
  original=reps[i];root=parents[i];merged=roots[root];p,x,y,w,h=placements[rootkeys[root]]
  # Fresh atlas integer origin for ORIGINAL requested rectangle (including gutters).
  ox=x+original[2]-merged[2];oy=y+original[3]-merged[3]
  mappings.append({'recipe':r,'page':p,'rect':[ox,oy,r[4]+4,r[5]+4],'merged_recipe':merged,'canonical_recipe':original})
 # Verify all original footprint pens including gutters through packed pages.
 checked=0
 for m in mappings:
  b,c,u,v,w,h=m['recipe'];p=m['page'];x,y,ww,hh=m['rect'];expected=rom.pens(b,c,(u-2)&4095,(v-2)&4095,w+4,h+4)
  assert np.array_equal(pages[p][y:y+hh,x:x+ww],expected),m
  checked+=ww*hh
 for i,page in enumerate(pages):(out/f'page-{i}.r8ui').write_bytes(page.tobytes())
 result={'label':label,'recipes':len(aliases),'unique_initial_contents':len(reps),'exact_duplicate_hits':exact_hits,'initial_padded_bytes':bytecount,'overlap_merges':len(savings),'merged_rectangles':len(roots),'merge_saved_padded_bytes':sum(savings),'unique_final_contents':len(contents),'page_count':len(pages),'page_dimensions':dims,'allocated_bytes':sum(p.nbytes for p in pages),'full_4096_page_bytes':len(pages)*4096**2,'content_bytes':sum(map(len,contents.values())),'packed_pen_texels_verified':checked,'packing_seconds':time.monotonic()-start,'files':{f'page-{i}.r8ui':{'bytes':p.nbytes,'sha256':digest(p.tobytes())} for i,p in enumerate(pages)}}
 (out/'mapping.json').write_bytes(encode(mappings));(out/'sizing.json').write_bytes(encode(result));return result,mappings,pages

