#!/usr/bin/env python3
"""Every packed recipe/gutter against ROM and captured palette; scalar address cross-check."""
import argparse,pathlib,sys,json,hashlib
import numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'assets'))
from asset_tool import ROM
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('race',type=pathlib.Path);p.add_argument('audit',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();rom=ROM(a.roms);m=json.loads((a.race/'manifest.json').read_bytes());mat=json.loads((a.race/'materials.json').read_bytes());variants=json.loads((a.audit/'variants.json').read_bytes());assert m['source_roms']==rom.sources and mat.keys()==variants.keys();pages=[np.memmap(a.race/f'page-{i}.rgba',mode='r',dtype=np.uint8,shape=(h,w,4)) for i,(w,h) in enumerate(m['pages'])];texels=scalar=0
for key,info in mat.items():
 bank,g,cm,u,v,w,h=info['recipe'];pal=np.frombuffer(bytes.fromhex(variants[key]['palette_rgb']),np.uint8).reshape(256,3);assert info['palette_sha256']==hashlib.sha256(pal.tobytes()).hexdigest();page=info['page'];x,y,ww,hh=info['rect'];assert [ww,hh]==[w+4,h+4];cut=pages[page][y:y+hh,x:x+ww];pens=rom.pens(bank,cm,(u-2)&4095,(v-2)&4095,ww,hh);assert np.array_equal(cut[:,:,:3],pal[pens]) and np.all(cut[:,:,3]==255);texels+=ww*hh
 for xx,yy in [(0,0),(ww-1,0),(0,hh-1),(ww-1,hh-1),(ww//2,hh//2)]:
  su=(u-2+xx)&4095;sv=((v-2+yy)&4095)|(bank<<12);i=((sv&0xfff0)<<4)|((su&0xff0)>>4);tile=int(rom.tiles[i]);at=int(rom.attr[i]);tx=su&15;ty=sv&15
  if at&4:tx=15-tx
  if at&2:ty=15-ty
  if at&8:tx,ty=ty,tx
  addr=tile*256+ty*16+tx;pen=int(rom.cg[addr]) if addr<len(rom.cg) else 0
  if cm&4:pen=(0xec+((cm&8)<<1)+((pen>>(2*(~cm&3)))&3))&255
  elif cm&2:pen=(0xe0+((cm&8)<<1)+((pen>>(4*(~cm&1)))&15))&255
  assert np.array_equal(cut[yy,xx,:3],pal[pen]);scalar+=1
 pu,pv=m['pages'][page];expected=np.array([1/pu,1/pv,(x+2+.5-u)/pu,(y+2+.5-v)/pv]);assert np.array_equal(info['uv'],expected)
report=dict(material_recipes=len(mat),full_source_and_gutter_texels=texels,independent_scalar_samples=scalar,errors=0,filter='nearest; source-aware two-texel gutters; mipmaps disabled')
a.out.write_text(json.dumps(report,indent=2)+'\n');print(report)
