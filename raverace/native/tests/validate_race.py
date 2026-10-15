#!/usr/bin/env python3
"""Validate complete native race against original-source packets, not frame counts alone."""
import argparse,pathlib,sys,json,hashlib,struct,collections
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'raverace/native/scene'))
from protocol import read
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('race',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args()
sha=lambda b:hashlib.sha256(b).hexdigest();m=json.loads((a.race/'manifest.json').read_bytes());assert m['format']=='rr-native-race' and m['version']==2
for name,info in m['files'].items():
 assert pathlib.Path(name).name==name;f=a.race/name
 assert f.stat().st_size==info['bytes']
 with f.open('rb') as stream:assert hashlib.file_digest(stream,'sha256').hexdigest()==info['sha256']
stream=(a.race/'frames.rsc').open('rb');assert struct.unpack('<4I',stream.read(16))==(0x33525352,2,3840,0);offsets=struct.unpack('<3841Q',stream.read(3841*8));assert offsets[0]==16+3841*8 and offsets[-1]==(a.race/'frames.rsc').stat().st_size
index=json.loads((a.race/'frames.json').read_bytes());assert len(index)==3840 and [r['tick'] for r in index]==list(range(1800,5640))
meshes=json.loads((a.race/'meshes.json').read_bytes());geo=np.frombuffer((a.race/'meshes.f32').read_bytes(),'<f4').reshape(-1,4);seen=set();quads=direct=0;maxupload=0;maxframe=0
for entry in index:
 tick=entry['tick'];stream.seek(offsets[tick-1800]);assert sha(stream.read(offsets[tick-1799]-offsets[tick-1800]))==entry['sha256'];f=read(a.capture/f'tick-{tick}.bin');b=(a.race/'frames'/f'{tick}.bin').read_bytes();assert sha(b)==entry['sha256'] and f['sha256']==entry['source_sha256'];magic,version,t,n,ng,nd,nh,pal,*bg=struct.unpack_from('<8I3f',b);assert magic==0x33504e52 and version==2 and t==tick and n==len(f['quads'])+len(f['direct']) and pal in m['palette_revisions'].values();assert len(b)==44+n*144+ng*32+nd*16+nh
 cmds=np.frombuffer(b,'<f4',n*36,44).reshape(n,36);groups=np.frombuffer(b,'<i4',ng*8,44+n*144).reshape(ng,8);off=44+n*144+ng*32;dyn=np.frombuffer(b,'<f4',nd*4,off).reshape(nd,4);off+=nd*16;hud=b[off:];assert len(hud)==16+0x20000+1024+9+1024;assert hud[16:16+0x1e000]==f['cgram'] and hud[16+0x1e000:16+0x20000]==f['text']
 q=f['quads'];d=len(f['direct']);identity=cmds[:,33].astype(np.int32);assert set(identity)==set(range(n)) and len(set(identity))==n
 expected_rank=sorted([(int(r[2]),d+i,d+i) for i,r in enumerate(q)]+[(((int(w[1])&4095)<<12)|(int(w[0])&4095),i,i) for i,w in enumerate(f['direct'])],reverse=True)
 depth={i:np.float32(.9-1.8*(rank+1)/(n+1)) for rank,(_,_,i) in enumerate(expected_rank)}
 consumed=0
 for g in groups:
  first,count,page,x,y,w,h,protect=map(int,g);assert first==consumed and count>0 and first+count<=n and 0<=page<len(m['pages']) and w>=0 and h>=0;consumed+=count
 assert consumed==n
 for cmd in cmds:
  order=int(cmd[33]);assert cmd[25]==depth[order];seen.add((tick,int(cmd[32])))
  if order>=d:
   r=q[order-d];assert int(cmd[32])==r[35] and int(cmd[34])==r[0] and int(cmd[35])==r[1]
   expected=np.column_stack([r[12:21].reshape(3,3).T,r[21:24]]).astype('<f4').ravel();assert np.array_equal(cmd[:12],expected);assert np.array_equal(cmd[20:24],r[36:60].reshape(4,6)[:,5].astype('<f4')/64)
   model=meshes[int(cmd[24])//8];assert model['code']==r[0] and model['packet']==r[1];local=r[36:60].reshape(4,6);v=geo[int(cmd[24]):int(cmd[24])+8].reshape(4,8);assert np.array_equal(v[:,:3],local[:,:3]) and np.array_equal(v[:,4:6],local[:,3:5]);quads+=1
  else:
   assert int(cmd[32])==-1-order and int(cmd[34])==-1 and int(cmd[35])==order;assert cmd[24]==-1-order*8;direct+=1
 maxupload=max(maxupload,n*144+nd*16);maxframe=max(maxframe,len(b))
report=dict(ticks=3840,source_quads_verified=quads,direct_quads_verified=direct,source_meshes=len(meshes),frame_local_instance_ids=len(seen),omitted_commands=0,checksum_or_source_or_priority_errors=0,max_dynamic_world_upload_bytes=maxupload,max_packet_bytes=maxframe,static_gpu_bytes=sum(w*h*4 for w,h in m['pages'])+(a.race/'meshes.f32').stat().st_size,palette_revisions=len(m['palette_revisions']))
a.out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
