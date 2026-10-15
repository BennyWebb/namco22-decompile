#!/usr/bin/env python3
"""Validate actual source images, independent pen addressing and fresh native UVs."""
import argparse,hashlib,importlib.util,json,pathlib
import numpy as np
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);a=p.parse_args()
spec=importlib.util.spec_from_file_location('asset',pathlib.Path(__file__).parents[1]/'assets/asset_tool.py');asset=importlib.util.module_from_spec(spec);spec.loader.exec_module(asset);r=asset.ROM(a.roms)
m=json.loads((a.pack/'manifest.json').read_bytes());assert m['format']=='rr-native-material-pages' and m['version']==1
for name,info in m['files'].items():
 assert pathlib.Path(name).name==name
 b=(a.pack/name).read_bytes();assert len(b)==info['bytes'] and hashlib.sha256(b).hexdigest()==info['sha256']
assert r.sources==m['rom_sources']
mat=json.loads((a.pack/'materials.json').read_bytes());pal=np.frombuffer((a.pack/'palette.rgb').read_bytes(),np.uint8).reshape(128,256,3);pages=[np.frombuffer((a.pack/f'page-{i}.rgba').read_bytes(),np.uint8).reshape(h,w,4) for i,(w,h) in enumerate(mat['pages'])]
def scalar(u,v,bank,cmode,group):
 u&=4095;v=(v&4095)|(bank<<12);index=((v&0xfff0)<<4)|((u&0xff0)>>4);tile=int(r.tiles[index]);at=int(r.attr[index]);x=u&15;y=v&15
 if at&4:x=15-x
 if at&2:y=15-y
 if at&8:x,y=y,x
 addr=tile*256+y*16+x;pen=int(r.cg[addr]) if addr<len(r.cg) else 0
 if cmode&4:pen=(0xec+((cmode&8)<<1)+((pen>>(2*(~cmode&3)))&3))&255
 elif cmode&2:pen=(0xe0+((cmode&8)<<1)+((pen>>(4*(~cmode&1)))&15))&255
 return [*map(int,pal[group,pen]),255]
texels=points=0;groups=set();tested_attrs=set()
for key,info in mat['materials'].items():
 bank,group,cmode,u,v,w,h=info['recipe'];g=info['gutter'];page=pages[info['page']];x,y,ww,hh=info['rect'];actual=page[y:y+hh,x:x+ww];pens=r.pens(bank,cmode,(u-g)&4095,(v-g)&4095,ww,hh)
 assert np.array_equal(actual[:,:,:3],pal[group][pens]) and np.all(actual[:,:,3]==255)
 texels+=ww*hh;groups.add(group)
 sx,sy,ox,oy=info['native_uv_affine']
 for px,py in [(u,v),(u+w-1,v),(u,v+h-1),(u+w-1,v+h-1),(u+w//2,v+h//2),(u-g,v-g),(u+w-1+g,v+h-1+g)]:
  native=np.asarray([px*sx+ox,py*sy+oy],np.float32);ix=int(np.floor(native[0]*page.shape[1]));iy=int(np.floor(native[1]*page.shape[0]));assert page[iy,ix].tolist()==scalar(px,py,bank,cmode,group);points+=1
# Address actual examples of every ROM orientation nibble, every packed cmode,
# source wrap and out-of-range tiles. These are correctness probes, not scenes.
for attr in range(16):
 locations=np.flatnonzero(r.attr==attr)
 if not len(locations):continue
 index=int(locations[len(locations)//2]);bank=index//65536;u=(index%256)*16;v=((index//256)%256)*16
 for cmode in range(16):
  image=r.pens(bank,cmode,u,v,16,16)
  for xx,yy in [(0,0),(15,0),(0,15),(15,15),(3,11)]:assert [*map(int,pal[28,image[yy,xx]]),255]==scalar(u+xx,v+yy,bank,cmode,28);points+=1
 tested_attrs.add(attr)
for u,v in [(-2,-2),(4094,4094),(4095,0)]:
 image=r.pens(1,0,u&4095,v&4095,5,5)
 for y in range(5):
  for x in range(5):assert [*map(int,pal[28,image[y,x]]),255]==scalar(u+x,v+y,1,0,28);points+=1
print(json.dumps(dict(material_recipes=len(mat['materials']),page_count=len(pages),verified_gutter_texels=texels,independent_samples=points,orientation_nibbles=sorted(tested_attrs),palette_groups=len(groups),uv_or_rgba_differences=0,filter='nearest; mipmaps not enabled'),sort_keys=True))
