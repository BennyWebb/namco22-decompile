#!/usr/bin/env python3
"""Exact-layout pen control and selectable sampling/precision shaders; private data."""
import pathlib,sys,json,struct,hashlib,shutil,numpy as np
root=pathlib.Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'raverace/native/assets'));from asset_tool import ROM
base=root/'raverace/native/generated/c1c';source=root/'raverace/native/generated/milestone4a-coverage/race-live-v2';rom=ROM(root/'raverace/native/generated/c1b/roms');mats=json.loads((source/'materials.json').read_bytes());dims=[list(map(int,l.split())) for l in (source/'race.meta').read_text().splitlines()[1:]]
control=base/'layout';control.mkdir(exist_ok=True);pages=[np.zeros((h,w),np.uint8) for w,h in dims];valid=[np.zeros_like(x,bool) for x in pages];overlap=conflicts=0
for v in sorted(mats.values(),key=lambda x:x['id']):
 b,g,c,u,y,w,h=v['recipe'];p=v['page'];x0,y0,ww,hh=v['rect'];pen=rom.pens(b,c,(u-2)&4095,(y-2)&4095,w+4,h+4);assert pen.shape==(hh,ww)
 region=pages[p][y0:y0+hh,x0:x0+ww];mask=valid[p][y0:y0+hh,x0:x0+ww];overlap+=int(mask.sum());conflicts+=int(np.count_nonzero(mask&(region!=pen)));region[~mask]=pen[~mask];mask[:]=True
# RGBA dedup may alias different pens that are colour-equivalent. Preserve the
# first pen, then prove actual palette lookup for every material, not pen identity.
data=(root/'raverace/native/generated/milestone4a-coverage/lookup/live.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',data);palettes={}
for i in range(nt):
 off=16+nm*12+i*800;row=struct.unpack_from('<8i',data,off);palettes[row[7]]=np.frombuffer(data,np.uint8,768,off+32).reshape(256,3)
rgba=[np.memmap(source/f'page-{i}.rgba',np.uint8,'r',shape=(h,w,4)) for i,(w,h) in enumerate(dims)];bad=0;badcoords=[set() for _ in pages]
for v in sorted(mats.values(),key=lambda x:x['id']):
 p=v['page'];x,y,w,h=v['rect'];rgb=palettes[v['id']][pages[p][y:y+h,x:x+w]];diff=np.any(rgb!=rgba[p][y:y+h,x:x+w,:3],axis=2);bad+=int(diff.sum());badcoords[p].update((int(x+dx),int(y+dy)) for dy,dx in np.argwhere(diff))
print('layout raw pen conflicts',conflicts,'palette lookup differences',bad,flush=True)
coords=[np.array(sorted(q),dtype=np.int32).reshape(-1,2) for q in badcoords];allowed=[np.ones((len(q),256),bool) for q in coords];constraints=[np.zeros(len(q),np.uint16) for q in coords]
for v in sorted(mats.values(),key=lambda x:x['id']):
 p=v['page'];x,y,w,h=v['rect'];q=coords[p];ids=np.flatnonzero((q[:,0]>=x)&(q[:,0]<x+w)&(q[:,1]>=y)&(q[:,1]<y+h))
 if not len(ids):continue
 colour=rgba[p][q[ids,1],q[ids,0],:3];possible=np.all(palettes[v['id']][None,:,:]==colour[:,None,:],axis=2);allowed[p][ids]&=possible;constraints[p][ids]+=1
impossible=[]
for p,q in enumerate(coords):
 for i,(x,y) in enumerate(q):
  hits=np.flatnonzero(allowed[p][i])
  if not len(hits):impossible.append({'page':p,'xy':[int(x),int(y)],'constraints':int(constraints[p][i])})
  else:pages[p][y,x]=hits[0]
print('layout constraints',sum(map(len,coords)),'unsatisfiable',len(impossible),flush=True)
(root/'raverace/benchmarks/native-c1c-20261016/layout-alias-constraints.json').write_text(json.dumps({'initial_pen_conflicts':conflicts,'initial_palette_differences':bad,'candidate_coordinates':sum(map(len,coords)),'unsatisfiable_pixels':len(impossible),'examples':impossible[:20]},indent=2)+'\n')
(base/'layout-alias-locations.json').write_text(json.dumps(impossible,indent=2)+'\n')
print('Writing explicitly conflicted first-pen layout for visibility attribution only',flush=True)
for i,x in enumerate(pages):(control/f'page-{i}.r8ui').write_bytes(x.tobytes())
(control/'atlas.meta').write_text(str(len(dims))+'\n'+''.join(f'{w} {h}\n' for w,h in dims));shutil.copyfile(source/'materials.bin',control/'materials.bin');(control/'groups.bin').write_bytes(b''.join(struct.pack('<I',v['recipe'][1]) for v in sorted(mats.values(),key=lambda v:v['id'])))
compact=base/'compact';compact.mkdir(exist_ok=True)
for p in (root/'raverace/native/generated/c1b/reference').iterdir():
 if p.is_file() and (p.suffix in ['.bin','.r8ui','.meta','.vert']):shutil.copyfile(p,compact/p.name)
for d in [compact,control]:
 for n in ['race_v3.vert','race_v3_near.vert']:shutil.copyfile(root/'raverace/native/generated/c1b/reference'/n,d/n)
 for sampler in ['texture','fetch']:
  for precision in ['highp','lowp']:
   src=(root/'raverace/native/generated/c1b/reference/index.frag').read_text().replace('uniform highp sampler2D palette;',f'uniform {precision} sampler2D palette;')
   if sampler=='texture':src=src.replace('texelFetch(image,xy,0).r','texture(image,texcoord).r')
   (d/f'index_{sampler}_{precision}.frag').write_text(src)
result={'valid_source_palette_control':False,'purpose':'explicitly conflicted first-pen visibility diagnostic','unsatisfiable_shared_texels':len(impossible),'materials':len(mats),'pages':dims,'layout_pen_overlap_texels':overlap,'layout_pen_conflicts':conflicts,'material_bytes_identical':(control/'materials.bin').read_bytes()==(source/'materials.bin').read_bytes(),'page_bytes':sum(x.nbytes for x in pages),'sources':rom.sources};(root/'raverace/benchmarks/native-c1c-20261016/layout-control.json').write_text(json.dumps(result,indent=2)+'\n');print('layout control prepared',len(pages),'pages; overlap',overlap,'conflicts',conflicts)
