#!/usr/bin/env python3
"""ROM-only bank-flattening feasibility audit. Emits hashes/counts, never ROM bytes."""
import argparse,collections,hashlib,json,pathlib,time,zipfile
import numpy as np

def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('rom_zip',type=pathlib.Path);p.add_argument('reference_pack',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
 start=time.monotonic();z=zipfile.ZipFile(a.rom_zip);sources={}
 def read(n,size):
  b=z.read(n);assert len(b)==size;(sources.update({n:{'bytes':len(b),'sha256':sha(b)}}));return b
 cg=np.frombuffer(b''.join(read(f'rv1cg{i}.{s}',0x200000) for i,s in enumerate(['1a','1c','1d','1e','1f','1j','1k','1n'])),np.uint8)
 tiles=np.frombuffer(read('rv1ccrl.5a',0x200000),'<u2').astype(np.uint32);nib=np.frombuffer(read('rv1ccrh.5c',0x80000),np.uint8)
 attr=np.empty(0x100000,np.uint8);attr[::2]=nib>>4;attr[1::2]=nib&15;tiles|=(attr.astype(np.uint32)&1)<<16
 def raw(bank,u,v,w,h):
  u=(u+np.arange(w,dtype=np.uint32))&4095;v=((v+np.arange(h,dtype=np.uint32))&4095)|(bank*4096)
  ix=((v[:,None]&0xfff0)<<4)|((u[None,:]&0xff0)>>4);at=attr[ix];tile=tiles[ix]
  x=np.broadcast_to(u[None,:]&15,ix.shape);y=np.broadcast_to(v[:,None]&15,ix.shape)
  x=np.where(at&4,15-x,x);y=np.where(at&2,15-y,y);xx=np.where(at&8,y,x);yy=np.where(at&8,x,y)
  off=tile*256+yy*16+xx
  return np.where(off<len(cg),cg[np.minimum(off,len(cg)-1)],0).astype(np.uint8)
 # Independent scalar decoder checks all banks and local coordinates, with exact
 # high-nibble-first, flips-before-transpose and invalid-tile semantics.
 checks=0;orientation=set()
 for bank in range(16):
  for k in range(4096):
   u=(k*1297+17)&4095;v=(k*193+31)&4095
   address=((bank*4096+v)//16)*256+u//16
   at=int(attr[address]);orientation.add(at&14);tile=int(tiles[address]);x=u%16;y=v%16
   if at&4:x=15-x
   if at&2:y=15-y
   if at&8:x,y=y,x
   off=tile*256+y*16+x;expected=int(cg[off]) if off<len(cg) else 0
   assert int(raw(bank,u,v,1,1)[0,0])==expected
   checks+=1
 mats=json.loads((a.reference_pack/'materials.json').read_text());recipes=sorted({tuple(m['recipe']) for m in mats.values()});banks=sorted({r[0] for r in recipes})
 # Full bank hashes prove distinct planes, including invalid tile zero semantics.
 planes=[]
 for bank in range(16):
  h=hashlib.sha256();nonzero=0
  for v in range(0,4096,64):
   b=raw(bank,0,v,4096,64);h.update(b.tobytes());nonzero+=int(np.count_nonzero(b))
  planes.append({'bank':bank,'sha256':h.hexdigest(),'bytes':4096**2,'nonzero_texels':nonzero})
 # A single unbanked image must return the same raw pen at an identical (u,v).
 # Find an explicit conflicting pair among actually referenced rectangles.
 conflict=None
 for i,r in enumerate(recipes):
  for s in recipes[i+1:]:
   if r[0]==s[0] or not r[0] or not s[0]:continue
   u=max(r[3],s[3]);v=max(r[4],s[4]);w=min(r[3]+r[5],s[3]+s[5])-u;h=min(r[4]+r[6],s[4]+s[6])-v
   if w<=0 or h<=0:continue
   x=raw(r[0],u,v,w,h);y=raw(s[0],u,v,w,h);diff=np.argwhere(x!=y)
   if len(diff):
    dy,dx=map(int,diff[0]);conflict={'recipe_a':r,'recipe_b':s,'uv':[u+dx,v+dy],'raw_pen_a':int(x[dy,dx]),'raw_pen_b':int(y[dy,dx]),'different_overlap_texels':len(diff)};break
  if conflict:break
 assert conflict,'Expected explicit cross-bank counterexample'
 dimensions=(a.reference_pack/'race.meta').read_text().splitlines()[1:];rgba=sum(int(l.split()[0])*int(l.split()[1])*4 for l in dimensions)
 usedunique=len({planes[b]['sha256'] for b in banks});allunique=len({x['sha256'] for x in planes})
 result={'status':'BLOCKED_DESIGN_DECISION','sources':sources,'reference_materials':len(mats),'reference_recipe_count':len(recipes),'reference_banks':banks,'recipes_per_bank':dict(collections.Counter(r[0] for r in recipes)),'planes':planes,'cross_bank_counterexample':conflict,'single_plane_bytes':4096**2,'reference_distinct_bank_bytes':usedunique*4096**2,'all_distinct_bank_bytes':allunique*4096**2,'palette_bytes':128*256*4,'reference_rgba_bytes':rgba,'bank_flattening_reduction_ratio':rgba/(usedunique*4096**2+128*256*4),'elapsed_seconds':time.monotonic()-start,'scalar_reference_checks':checks,'orientations_checked':sorted(orientation),'device_runs':0,'rom_data_emitted':False}
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['status','reference_banks','reference_distinct_bank_bytes','all_distinct_bank_bytes','reference_rgba_bytes','bank_flattening_reduction_ratio','cross_bank_counterexample','elapsed_seconds']},indent=2))
if __name__=='__main__':main()
