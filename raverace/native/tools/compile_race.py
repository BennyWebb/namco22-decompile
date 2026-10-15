#!/usr/bin/env python3
"""Compile actual captured race into resident source meshes/materials + dynamic commands."""
import argparse,sys,pathlib,json,hashlib,struct,math,collections
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3];sys.path[:0]=[str(root/'raverace/native/scene'),str(root/'raverace/native/assets')]
from protocol import read,recipes
from asset_tool import ROM
from consolidate_materials import pack
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('capture',type=pathlib.Path);p.add_argument('audit',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--reuse-materials',type=pathlib.Path);a=p.parse_args()
assert not a.out.exists();assert a.out.resolve().is_relative_to(root/'raverace/native/generated');a.out.mkdir();(a.out/'frames').mkdir()
sha=lambda b:hashlib.sha256(b).hexdigest();enc=lambda x:(json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
rom=ROM(a.roms)
if a.reuse_materials:
 import os
 previous=json.loads((a.reuse_materials/'manifest.json').read_bytes());assert previous['source_roms']==rom.sources
 dims=previous['pages'];mapping=json.loads((a.reuse_materials/'materials.json').read_bytes());placements={v['content'] for v in mapping.values()}
 for i in range(len(dims)):
  name=f'page-{i}.rgba';b=(a.reuse_materials/name).read_bytes();assert sha(b)==previous['files'][name]['sha256'];os.link(a.reuse_materials/name,a.out/name)
else:
 variants=json.loads((a.audit/'variants.json').read_bytes());contents={};mapping={}
 for key,v in sorted(variants.items()):
  bank,g,cm,u,v0,w,h=v['recipe'];pens=rom.pens(bank,cm,(u-2)&4095,(v0-2)&4095,w+4,h+4);pal=np.frombuffer(bytes.fromhex(v['palette_rgb']),np.uint8).reshape(256,3);rgba=np.empty((*pens.shape,4),np.uint8);rgba[:,:,:3]=pal[pens];rgba[:,:,3]=255;b=rgba.tobytes();content=sha(struct.pack('<2I',w+4,h+4)+b);contents.setdefault(content,rgba);mapping[key]=dict(recipe=v['recipe'],content=content,palette_sha256=sha(pal.tobytes()))
 placements,dims=pack([(k,v.shape[1],v.shape[0]) for k,v in contents.items()],4096)
 print(json.dumps(dict(unique_content=len(contents),pages=dims,page_bytes=sum(w*h*4 for w,h in dims),content_bytes=sum(v.nbytes for v in contents.values()))),flush=True)
 for page,(w,h) in enumerate(dims):
  image=np.zeros((h,w,4),np.uint8)
  for key,v in contents.items():
   pi,x,y,ww,hh=placements[key]
   if pi==page:image[y:y+hh,x:x+ww]=v
  (a.out/f'page-{page}.rgba').write_bytes(image.tobytes())
 del contents
 for key,v in mapping.items():
  pi,x,y,w,h=placements[v['content']];v['page']=pi;v['rect']=[x,y,w,h];u,v0=v['recipe'][3:5];pw,ph=dims[pi];v['uv']=[1/pw,1/ph,(x+2+.5-u)/pw,(y+2+.5-v0)/ph]
geometry=[];mesh_ids={};frames=[];pal_ids={};maxcommands=0;meshROMerrors=0
for tick in range(1800,5640):
 f=read(a.capture/f'tick-{tick}.bin');q=f['quads'];r=recipes(f);pal=np.frombuffer(f['palette'],np.uint8).reshape(3,128,256).transpose(1,2,0).copy();ph=sha(f['palette']);pal_ids.setdefault(ph,len(pal_ids));gh={g:sha(pal[g].tobytes()) for g in {x[1] for x in r}}
 keys=[sha(json.dumps([recipe,gh[recipe[1]]],separators=(',',':')).encode()) for recipe in r];commands=[]
 for i,row in enumerate(q):
  if row[0]==5 or row[7]!=0:raise ValueError('unimplemented dynamic point RAM/solid: stop, do not omit')
  local=row[36:60].reshape(4,6);identity=(int(row[0]),int(row[1]));pos=local[:,:3]
  assert np.array_equal(pos,rom.words[row[1]+8:row[1]+20].reshape(4,3)),'source local ROM mismatch'
  if identity not in mesh_ids:
   mesh_ids[identity]=len(geometry);geometry.append(np.column_stack([local[:,:3],np.zeros(4),local[:,3:5],np.zeros((4,2))]).astype('<f4').reshape(8,4))
  info=mapping[keys[i]];cam=[row[26]/320-1,1-row[27]/240,math.ldexp(int(row[24]),-int(row[25]))/320,math.ldexp(int(row[24]),-int(row[25]))/240]
  matrix=row[12:21].reshape(3,3);trans=row[21:24];l,rr,t,b=map(int,row[28:32]);box=[max(0,l),479-min(479,b),min(639,rr)-max(0,l)+1,min(479,b)-max(0,t)+1]
  oracle=row[60:].reshape(4,7);xy=oracle[:,:2].astype(np.int64);protect=False
  if np.all(oracle[:,6]):
   for ii,jj,kk in [(0,1,2),(0,2,3)]:
    ab=xy[jj]-xy[ii];ac=xy[kk]-xy[ii];protect|=bool(ab[0]*ac[1]-ab[1]*ac[0]==0)
  cz=int(row[9])&3;ff=0 if (int(row[3])>>8)&128 else f['czram'][((cz<<13)|(int(row[10])&8191))&32767];cc=cz&f['mixer'][0x84+cz];fog=[f['mixer'][offset+cc]/255 for offset in [0x100,0x180,0x200]]+[ff/255]
  vals=[*matrix[:,0],trans[0],*matrix[:,1],trans[1],*matrix[:,2],trans[2],*cam,*info['uv'],*(local[:,5]/64),mesh_ids[identity]*8,0,int((row[6]&7)==1),int(row[34]),*fog]
  vals.extend([int(row[35]),len(f['direct'])+i,int(row[0]),int(row[1])]);assert len(vals)==36
  commands.append(dict(values=vals,page=info['page'],box=box,protected=protect,depth=int(row[2]),order=len(f['direct'])+i,code=int(row[0]),packet=int(row[1])))
 dynamic=[]
 for i,words in enumerate(f['direct']):
  info=mapping[keys[len(q)+i]];verts=[];bri=[]
  for j in range(4):
   w=words[4+j*6:10+j*6];sx=struct.unpack('<h',struct.pack('<H',w[2]))[0];sy=struct.unpack('<h',struct.pack('<H',w[3]))[0];ooz=math.ldexp(int(w[5]),-max(0,46-(int(w[4])&63))) if w[5] else 65536;z=min(2000000000,max(1,int(1/ooz)))
   verts.extend([[sx*z,-sy*z,z,0],[int(w[0])&4095,int(w[1])&4095,0,0]]);bri.append(int(w[4]>>8)/64)
  dynamic.extend(verts)
  vals=[32768,0,0,0,0,32768,0,0,0,0,32768,0,0,0,1/320,1/240,*info['uv'],*bri,-1-i*8,0,int((int(words[4]>>12)&7)==1),0,0,0,0,0]
  vals.extend([-1-i,i,-1,i])
  commands.append(dict(values=vals,page=info['page'],box=[0,0,640,480],protected=False,depth=((int(words[1])&4095)<<12)|(int(words[0])&4095),order=i,code=-1,packet=i))
 commands.sort(key=lambda v:(-v['depth'],-v['order']))
 for rank,c in enumerate(commands):c['values'][25]=.9-1.8*(rank+1)/(len(commands)+1)
 # Stable identity/priority is stored before batching; only compatible opaque parts reorder.
 commands.sort(key=lambda v:(v['protected'],v['page'],v['box']))
 groups=[]
 for i,c in enumerate(commands):
  if groups and not c['protected'] and not groups[-1][7] and groups[-1][2]==c['page'] and groups[-1][3:7]==c['box']:groups[-1][1]+=1
  else:groups.append([i,1,c['page'],*c['box'],int(c['protected'])])
 maxcommands=max(maxcommands,len(commands));bgbase=(f['mixer'][4]<<8)&0x7f00;bg=[f['palette'][bgbase+255+plane*32768]/255 for plane in range(3)]
 # Captured HUD state with original gamma/fade LUT, immutable per-tick data.
 mix=f['mixer'];base=(mix[7]<<8)&0x7f00;hudpal=bytearray()
 for k in range(256):hudpal.extend([f['palette'][base+k+c*32768] for c in range(3)]+[255])
 lut=bytearray()
 gammas=[(a.roms/name).read_bytes() for name in ['rr1gam.2d','rr1gam.3d','rr1gam.4d']]
 for v in range(256):
  for c in range(3):
   fade=mix[0x11+c*2]*256+mix[0x12+c*2];value=1 if fade>256 and v==0 else v;lut.append(gammas[c][min(255,(value*fade)>>8)])
  lut.append(255)
 at=f['attributes'];sx=((at[0]*256+at[1])-0x35c)&1023;sy=(at[2]*256+at[3])&1023;shadow=mix[0]&1
 hud=struct.pack('<4I',0x48554431,sx,sy,shadow)+f['cgram']+f['text']+hudpal+mix[8:17]+lut
 header=struct.pack('<8I3f',0x33504e52,2,tick,len(commands),len(groups),len(dynamic),len(hud),pal_ids[ph],*bg)
 blob=header+np.array([c['values'] for c in commands],'<f4').tobytes()+np.array(groups,'<i4').tobytes()+np.array(dynamic,'<f4').tobytes()+hud
 dest=a.out/'frames'/f'{tick}.bin';dest.write_bytes(blob);frames.append(dict(tick=tick,primitives=len(commands),groups=len(groups),pages=sorted({c['page'] for c in commands}),sha256=sha(blob),bytes=len(blob),source_sha256=f['sha256'],palette_revision=pal_ids[ph]));
 if tick%600==0:print('compiled',tick,flush=True)
geo=np.concatenate(geometry);width=1024;height=math.ceil(len(geo)/width);image=np.zeros((height*width,4),'<f4');image[:len(geo)]=geo;(a.out/'meshes.f32').write_bytes(image.tobytes())
(a.out/'materials.json').write_bytes(enc(mapping));(a.out/'frames.json').write_bytes(enc(frames));(a.out/'meshes.json').write_bytes(enc([dict(code=k[0],packet=k[1],texel=v*8) for k,v in mesh_ids.items()]))
(a.out/'race.meta').write_text(f'RRRACE2 {len(dims)} {width} {height} {maxcommands}\n'+''.join(f'{w} {h}\n' for w,h in dims));
manifest=dict(format='rr-native-race',version=2,first_tick=1800,last_tick=5639,frame_count=3840,pages=dims,mesh_dimensions=[width,height],source_meshes=len(geometry),material_recipes=len(mapping),rgba_content=len(placements),max_commands=maxcommands,palette_revisions=pal_ids,source_roms=rom.sources,frame_index_sha256=sha((a.out/'frames.json').read_bytes()),files={f.name:dict(bytes=f.stat().st_size,sha256=sha(f.read_bytes())) for f in sorted(a.out.iterdir()) if f.is_file()});(a.out/'manifest.json').write_bytes(enc(manifest));print(json.dumps({k:v for k,v in manifest.items() if k not in ['files','source_roms','palette_revisions']},indent=2))
