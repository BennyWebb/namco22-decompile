#!/usr/bin/env python3
"""Lossless v2 -> v3 native command compaction with explicit conservative culls."""
import argparse,pathlib,json,struct,hashlib,zlib,sys,os
import numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'))
from protocol import read,recipes
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);p.add_argument('capture',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[3];assert a.out.resolve().is_relative_to(repo/'raverace/native/generated') and not a.out.exists();a.out.mkdir()
sha=lambda b:hashlib.sha256(b).hexdigest();enc=lambda x:(json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
m=json.loads((a.source/'manifest.json').read_bytes());assert m['version']==2
for name,info in m['files'].items():
 with (a.source/name).open('rb') as f:assert hashlib.file_digest(f,'sha256').hexdigest()==info['sha256']
for name in ['meshes.f32','meshes.json',*['page-%d.rgba'%i for i in range(len(m['pages']))]]:os.link(a.source/name,a.out/name)
materials=json.loads((a.source/'materials.json').read_bytes());keys=sorted(materials);material_map={(tuple(materials[k]['recipe']),materials[k]['palette_sha256']):i for i,k in enumerate(keys)}
matbytes=b''.join(struct.pack('<4f4I',*materials[k]['uv'],materials[k]['page'],0,0,0) for k in keys);(a.out/'materials.bin').write_bytes(matbytes);(a.out/'materials.json').write_bytes(enc(materials))
fonts=[];styles=[];font_ids={};style_ids={};frames=[];offsets=[16+3841*8];total_in=total_out=0;maxima=dict(raw=0,compressed=0,instances=0,visible=0,groups=0);reason_counts=[0]*6
quad_dtype=np.dtype([('mesh','<i4'),('instance','<u2'),('material','<u2'),('depth','<f4'),('brightness','<u2',(4,)),('source_instance','<u2'),('order','<u2'),('flags','<u2'),('reserved','<u2'),('zsort','<u4')]);assert quad_dtype.itemsize==32
index=json.loads((a.source/'frames.json').read_bytes())
with (a.out/'frames.rsc').open('xb') as output:
 output.write(struct.pack('<4I',0x33525352,3,3840,1));output.write(bytes(3841*8))
 for entry in index:
  tick=entry['tick'];b=(a.source/'frames'/f'{tick}.bin').read_bytes();assert sha(b)==entry['sha256'];h=struct.unpack_from('<8I3f',b);n,ng,nd,nh=h[3:7];old=np.frombuffer(b,'<f4',n*36,44).reshape(n,36);oldgroups=np.frombuffer(b,'<i4',ng*8,44+n*144).reshape(ng,8);off=44+n*144+ng*32;dynamic=b[off:off+nd*16];hud=b[off+nd*16:];assert len(hud)==nh
  f=read(a.capture/f'tick-{tick}.bin');assert f['sha256']==entry['source_sha256'];q=f['quads'];d=len(f['direct']);assert len(q)+d==n
  source_recipes=recipes(f);rgb=np.frombuffer(f['palette'],np.uint8).reshape(3,128,256).transpose(1,2,0).copy();palette_hashes={g:sha(rgb[g].tobytes()) for g in {r[1] for r in source_recipes}}
  # Analytic float32 error enclosure for GPU Q15 dot, floor and translation.
  local=q[:,36:60].reshape(-1,4,6)[:,:,:3].astype(np.int64);mat=q[:,12:21].reshape(-1,3,3).astype(np.int64);tr=q[:,21:24].astype(np.int64)
  xyz=(np.einsum('nvr,nrc->nvc',local,mat)>>15)+tr[:,None,:]
  absolute=np.einsum('nvr,nrc->nvc',np.abs(local),np.abs(mat)).astype(np.float64)
  error=absolute/(32768*1048576)+np.abs(tr[:,None,:])/1048576+2
  low=xyz-error;high=xyz+error;safe=(low[:,:,2]>1).all(1)
  zoom=np.ldexp(q[:,24].astype(float),-q[:,25]);cx=q[:,26,None];cy=q[:,27,None]
  denom=np.maximum(low[:,:,2],1e-12);denom2=np.maximum(high[:,:,2],1e-12)
  def interval(c):
   combos=np.stack([low[:,:,c]/denom,low[:,:,c]/denom2,high[:,:,c]/denom,high[:,:,c]/denom2]);return combos.min(0)*zoom[:,None],combos.max(0)*zoom[:,None]
  xl,xh=interval(0);yl,yh=interval(1);xmin=cx+xl;xmax=cx+xh;ymin=cy-yh;ymax=cy-yl;clips=q[:,28:32]
  reasons=np.zeros(len(q),np.uint16)
  for why,test in [(1,(xmax<clips[:,0,None]-4).all(1)),(2,(xmin>clips[:,1,None]+4).all(1)),(3,(ymax<clips[:,2,None]-4).all(1)),(4,(ymin>clips[:,3,None]+4).all(1))]:reasons[(reasons==0)&safe&test]=why
  font=hud[16:16+0x1e000];style=hud[:16]+hud[16+0x20000:];text=hud[16+0x1e000:16+0x20000]
  for blob,ids,values in [(font,font_ids,fonts),(style,style_ids,styles)]:
   key=sha(blob)
   if key not in ids:ids[key]=len(values);values.append(blob)
  fi=font_ids[sha(font)];si=style_ids[sha(style)];assert fi<16 and si<4096
  boxes=np.zeros((n,4),'<i4');protect=np.zeros(n,np.uint16)
  for g in oldgroups:boxes[g[0]:g[0]+g[1]]=g[2+1:7];protect[g[0]:g[0]+g[1]]=g[7]
  instances=[];instance_ids={};records=np.zeros(n,quad_dtype);gpu=[];pages=[]
  for i,c in enumerate(old):
   order=int(c[33]);slot=int(c[32]);key=(slot,c[:16].tobytes(),boxes[i].tobytes())
   if key not in instance_ids:instance_ids[key]=len(instances);instances.append(c[:16].tobytes()+boxes[i].tobytes())
   inst=instance_ids[key];recipe=source_recipes[order-d if order>=d else len(q)+order];material=material_map[(recipe,palette_hashes[recipe[1]])];assert np.array_equal(c[16:20],np.array(materials[keys[material]]['uv'],'<f4'));reason=int(reasons[order-d]) if order>=d else 0
   if boxes[i,2]<=0 or boxes[i,3]<=0:reason=5
   flags=int(c[26])|(int(c[27])<<1)|(int(protect[i])<<2)|(reason<<8)
   bri=c[20:24]*64;assert np.all(bri==np.floor(bri)) and np.all((bri>=0)&(bri<=65535))
   zs=int(q[order-d,2])&0xffffff if order>=d else ((int(f['direct'][order,1])&4095)<<12)|(int(f['direct'][order,0])&4095)
   records[i]=(int(c[24]),inst,material,c[25],bri.astype(np.uint16),slot if slot>=0 else 65535,order,flags,0,zs)
   reason_counts[reason]+=1
   if not reason:gpu.append(i)
  assert len(instances)<=512
  gpu.sort(key=lambda i:((int(records[i]['flags'])>>1)&1,(int(records[i]['flags'])>>2)&1,materials[keys[int(records[i]['material'])]]['page'],tuple(boxes[i])))
  groups=[]
  for first,i in enumerate(gpu):
   r=records[i];near=(int(r['flags'])>>1)&1;protected=(int(r['flags'])>>2)&1;page=materials[keys[int(r['material'])]]['page'];mode=near|(protected<<1);box=list(map(int,boxes[i]))
   if groups and not protected and groups[-1][2:]==[page,mode,*box]:groups[-1][1]+=1
   else:groups.append([first,1,page,mode,*box])
  visible=np.array(gpu,'<u2').tobytes();visible+=bytes((-len(visible))%4)
  raw=struct.pack('<12I3f',0x33504e52,3,tick,n,len(gpu),len(groups),len(instances),nd,fi,si,h[7],0,*h[8:])+b''.join(instances)+records.tobytes()+visible+np.array(groups,'<i4').tobytes()+dynamic+text
  compressed=zlib.compress(raw,1);assert len(raw)<=524288 and len(compressed)<=524288
  output.write(compressed);offsets.append(offsets[-1]+len(compressed));total_in+=len(b);total_out+=len(compressed)
  for k,v in [('raw',len(raw)),('compressed',len(compressed)),('instances',len(instances)),('visible',len(gpu)),('groups',len(groups))]:maxima[k]=max(maxima[k],v)
  frames.append(dict(tick=tick,source=entry['source_sha256'],v2_sha256=entry['sha256'],raw_sha256=sha(raw),compressed_sha256=sha(compressed),raw_bytes=len(raw),compressed_bytes=len(compressed),source_commands=n,visible_commands=len(gpu),culled=n-len(gpu),palette_revision=h[7]))
  if tick%600==0:print('compacted',tick,flush=True)
 output.seek(16);output.write(struct.pack('<3841Q',*offsets))
(a.out/'hud-fonts.bin').write_bytes(b''.join(fonts));(a.out/'hud-styles.bin').write_bytes(b''.join(styles));(a.out/'frames.json').write_bytes(enc(frames));(a.out/'race.meta').write_text(f'RRRACE3 {len(m["pages"])} {m["mesh_dimensions"][0]} {m["mesh_dimensions"][1]} {len(keys)} {len(fonts)} {len(styles)}\n'+''.join(f'{w} {h}\n' for w,h in m['pages']))
files={}
for f in sorted(a.out.iterdir()):
 with f.open('rb') as stream:files[f.name]=dict(bytes=f.stat().st_size,sha256=hashlib.file_digest(stream,'sha256').hexdigest())
manifest=dict(format='rr-native-race',version=3,source_v2_manifest_sha256=sha((a.source/'manifest.json').read_bytes()),pages=m['pages'],source_roms=m['source_roms'],quad_stride=32,instance_stride=80,frame_count=3840,preload_ceiling_bytes=512*1024*1024,source_meshes=m['source_meshes'],maxima=maxima,cull_reason_counts=reason_counts,fonts=len(fonts),styles=len(styles),source_payload_bytes=total_in,compressed_payload_bytes=total_out,files=files)
(a.out/'manifest.json').write_bytes(enc(manifest));print(json.dumps({k:v for k,v in manifest.items() if k not in ['files','source_roms']},indent=2))
