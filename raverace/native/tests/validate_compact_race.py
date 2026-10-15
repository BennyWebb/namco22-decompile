#!/usr/bin/env python3
"""Lossless v2 correspondence, source accounting, conservative cull proof and bounds."""
import argparse,pathlib,sys,json,hashlib,struct,zlib
import numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'))
from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);p.add_argument('compact',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();sha=lambda b:hashlib.sha256(b).hexdigest();m=json.loads((a.compact/'manifest.json').read_bytes());assert m['version']==3 and m['format']=='rr-native-race' and sha((a.source/'manifest.json').read_bytes())==m['source_v2_manifest_sha256']
for name,info in m['files'].items():
 assert pathlib.Path(name).name==name;f=a.compact/name;assert f.stat().st_size==info['bytes']
 with f.open('rb') as stream:assert hashlib.file_digest(stream,'sha256').hexdigest()==info['sha256']
index=json.loads((a.compact/'frames.json').read_bytes());assert [f['tick'] for f in index]==list(range(1800,5640));reader=Reader(a.compact/'frames.rsc');materials=np.frombuffer((a.compact/'materials.bin').read_bytes(),'<f4').reshape(-1,8);pages=materials[:,4].copy().view('<u4');geo=np.frombuffer((a.compact/'meshes.f32').read_bytes(),'<f4').reshape(-1,4);fonts=(a.compact/'hud-fonts.bin').read_bytes();styles=(a.compact/'hud-styles.bin').read_bytes();counts=np.zeros(6,np.int64);commands=instances=0;negative={}
for entry in index:
 tick=entry['tick'];f=reader.read(tick);assert sha(f['raw'])==entry['raw_sha256'] and sha(f['compressed'])==entry['compressed_sha256'];h=f['header'];n,nv,ng,ni,nd,fi,si=h[3:10];q=f['quads'];ins=f['instances'];v=f['visible'];groups=f['groups'];b=(a.source/'frames'/f'{tick}.bin').read_bytes();assert sha(b)==entry['v2_sha256'];oldh=struct.unpack_from('<8I3f',b);old=np.frombuffer(b,'<f4',n*36,44).reshape(n,36);og=np.frombuffer(b,'<i4',oldh[4]*8,44+n*144).reshape(-1,8);assert oldh[3]==n and oldh[5]==nd and oldh[7]==h[10] and oldh[8:]==h[12:]
 boxes=np.zeros((n,4),'<i4');old_pages=np.zeros(n,'<u4');protected=np.zeros(n,bool)
 for g in og:boxes[g[0]:g[0]+g[1]]=g[3:7];old_pages[g[0]:g[0]+g[1]]=g[2];protected[g[0]:g[0]+g[1]]=g[7]
 assert np.array_equal(q['mesh'],old[:,24]) and np.array_equal(q['depth'],old[:,25]);assert np.array_equal(q['brightness'].astype('<f4')/64,old[:,20:24]);assert np.array_equal(q['order'],old[:,33]);assert set(q['order'])==set(range(n))
 assert np.array_equal(q['source_instance'],np.where(old[:,32]<0,65535,old[:,32]));assert np.all(q['instance']<ni) and np.all(q['material']<len(materials))
 assert np.array_equal(ins[q['instance'],:16],old[:,:16]);assert np.array_equal(ins[q['instance'],16:].copy().view('<i4'),boxes);assert np.array_equal(materials[q['material'],:4],old[:,16:20]);assert np.array_equal(pages[q['material']],old_pages)
 flags=q['flags'];assert np.all(q['reserved']==0) and np.array_equal(flags&1,old[:,26]) and np.array_equal((flags>>1)&1,old[:,27]) and np.array_equal((flags>>2)&1,protected)
 reasons=flags>>8;assert np.all(reasons<=5);counts+=np.bincount(reasons,minlength=6);assert set(map(int,v))==set(np.flatnonzero(reasons==0)) and len(set(map(int,v)))==nv
 off=44+n*144+oldh[4]*32;assert f['dynamic'].tobytes()==b[off:off+nd*16];hud=b[off+nd*16:];assert fonts[fi*122880:(fi+1)*122880]==hud[16:122896];assert styles[si*2073:(si+1)*2073]==hud[:16]+hud[131088:];assert f['text']==hud[122896:131088]
 position=0
 for g in groups:
  first,count,page,mode,*box=map(int,g);assert first==position and count>0 and first+count<=nv;position+=count;items=v[first:first+count];assert np.all(pages[q['material'][items]]==page) and np.all(boxes[items]==box) and np.all(((flags[items]>>1)&3)==mode);assert not(mode&2) or count==1
 assert position==nv
 # Check culls against independently reconstructed GPU-camera interval bounds.
 culled=np.flatnonzero((reasons>0)&(reasons<5));r=q[culled];assert np.all(r['mesh']>=0)
 if len(r):
  points=geo[r['mesh'][:,None]+np.arange(4)[None,:]*2,:3].astype(np.int64);state=ins[r['instance'],:16].reshape(-1,4,4);matrix=state[:,:3,:3].transpose(0,2,1).astype(np.int64);tr=state[:,:3,3].astype(np.int64);exact=(np.einsum('nvr,nrc->nvc',points,matrix)>>15)+tr[:,None,:];error=np.einsum('nvr,nrc->nvc',np.abs(points),np.abs(matrix)).astype(float)/(32768*1048576)+np.abs(tr[:,None,:])/1048576+2;lo=exact-error;hi=exact+error;assert np.all(lo[:,:,2]>1)
  def interval(c):
   values=np.stack([lo[:,:,c]/lo[:,:,2],lo[:,:,c]/hi[:,:,2],hi[:,:,c]/lo[:,:,2],hi[:,:,c]/hi[:,:,2]]);return values.min(0),values.max(0)
  xl,xh=interval(0);yl,yh=interval(1);cam=state[:,3,:];xmin=(cam[:,None,0]+cam[:,None,2]*xl+1)*320;xmax=(cam[:,None,0]+cam[:,None,2]*xh+1)*320;ymin=(1-cam[:,None,1]-cam[:,None,3]*yh)*240;ymax=(1-cam[:,None,1]-cam[:,None,3]*yl)*240;box=boxes[culled];why=reasons[culled]
  proof={1:(xmax<box[:,0,None]-2).all(1),2:(xmin>box[:,0,None]+box[:,2,None]-1+2).all(1),3:(ymax<480-box[:,1,None]-box[:,3,None]-2).all(1),4:(ymin>479-box[:,1,None]+2).all(1)}
  for reason,test in proof.items():assert np.all(test[why==reason]),(tick,reason)
 empty=reasons==5;assert np.all((boxes[empty,2]<=0)|(boxes[empty,3]<=0));commands+=n;instances+=ni
report=dict(ticks=3840,source_commands=commands,instance_records=instances,source_or_material_or_priority_errors=0,cull_reason_counts=counts.tolist(),cull_proof='Original local vertices + compact instance matrices + float-error interval projected by native camera; only wholly outside declared scissor with guard margin.',fonts=len(fonts)//122880,styles=len(styles)//2073,all_hud_states_byte_identical=True)
a.out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
