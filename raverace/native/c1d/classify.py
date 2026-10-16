#!/usr/bin/env python3
"""Every residual classified from actual nearest-sampled GPU source identities."""
import pathlib,json,csv,struct,hashlib,sys,collections,argparse
p=argparse.ArgumentParser();p.add_argument("--proof-mode",choices=["all","rgba","index"],default="all");args=p.parse_args()
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1d';ev=root/'raverace/benchmarks/native-c1d-20261016';source=root/'raverace/native/generated/milestone4a-coverage/race-live-v2';mats=json.loads((source/'materials.json').read_bytes());byid=sorted(mats.values(),key=lambda x:x['id']);recipes=np.array([m['recipe'] for m in byid],np.int64)
sys.path.insert(0,str(root/'raverace/native/assets'));from asset_tool import ROM
rom=ROM(root/'raverace/native/generated/c1b/roms');data=(root/'raverace/native/generated/milestone4a-coverage/lookup/live.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',data);pal=np.empty((nt,256,3),np.uint8)
for i in range(nt):
 off=16+nm*12+i*800;row=struct.unpack_from('<8i',data,off);pal[row[7]]=np.frombuffer(data,np.uint8,768,off+32).reshape(256,3)
def pens(b,u,v,c):
 ix=(((b*4096+v)&0xfff0)<<4)|((u&0xff0)>>4);t=rom.tiles[ix];a=rom.attr[ix];x=u&15;y=v&15;x=np.where(a&4,15-x,x);y=np.where(a&2,15-y,y);xx=np.where(a&8,y,x);yy=np.where(a&8,x,y);off=t*256+yy*16+xx;raw=np.where(off<len(rom.cg),rom.cg[np.minimum(off,len(rom.cg)-1)],0).astype(np.uint32)
 sh=np.where(c&4,2*(~c&3),np.where(c&2,4*(~c&1),0));mask=np.where(c&4,3,np.where(c&2,15,255));add=np.where(c&4,0xec+((c&8)<<1),np.where(c&2,0xe0+((c&8)<<1),0));return ((add+((raw>>sh)&mask))&255).astype(np.uint8)
checks=[];cache={}
for mode in (['rgba','index'] if args.proof_mode=='all' else [args.proof_mode]):
 for p in sorted((base/f'provenance-{mode}/images').glob('*.provenance.u32')):
  tick=int(p.name.split('.')[0].split('-')[1]);meta=np.fromfile(p,'<u4').reshape(480,640,4);cache[mode,tick]=meta;covered=meta[:,:,3]>0;ids=meta[:,:,3][covered].astype(int)-1;assert np.all(ids<len(byid));r=recipes[ids];b,u,v=meta[:,:,:3][covered].T.astype(np.int64);assert np.array_equal(b,r[:,0]);pen=pens(b,u,v,r[:,2]);expected=pal[ids,pen]
  raw=np.fromfile(p.with_name(f'tick-{tick}.texel.rgba'),np.uint8).reshape(480,640,4);actual=raw[covered,:3];errors=int(np.count_nonzero(np.any(expected!=actual,axis=1)))
  proof=p.with_name(f'tick-{tick}.proof.rgba').read_bytes();normal=(base/f'colour-{mode}/images'/f'tick-{tick}.world.rgba').read_bytes();proof_errors=int(np.count_nonzero(np.any(np.frombuffer(proof,np.uint8).reshape(-1,4)!=np.frombuffer(normal,np.uint8).reshape(-1,4),axis=1)))
  checks.append({'mode':mode,'tick':tick,'covered_pixels':int(covered.sum()),'ROM_palette_texel_mismatches':errors,'MRT_colour_proof_mismatches':proof_errors,'raw_alpha_mismatches':int(np.count_nonzero(raw[covered,3]!=255)),'provenance_sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(ev/('capture-proof.json' if args.proof_mode=='all' else 'capture-proof-'+args.proof_mode+'.json')).write_text(json.dumps(checks,indent=2)+'\n');assert len(checks)==(36 if args.proof_mode=='all' else 18)
assert all(x['ROM_palette_texel_mismatches']==x['MRT_colour_proof_mismatches']==0 for x in checks),checks
if args.proof_mode!='all':
 print(args.proof_mode,'PASS 18 colour and ROM/palette texel proofs');raise SystemExit(0)
rows=[];events=[];global_issues=[]
for p in sorted((base/'colour-rgba/images').glob('*.rgba')):
 name=p.name;tick=int(name.split('.')[0].split('-')[1]);a=np.fromfile(p,np.uint8).reshape(480,640,4);b=np.fromfile(base/'colour-index/images'/name,np.uint8).reshape(480,640,4);diff=np.any(a!=b,axis=2);old=cache['rgba',tick];new=cache['index',tick];o=old[diff].astype(np.int64);n=new[diff].astype(np.int64);ids=o[:,3]-1;covered=(o[:,3]>0)&(n[:,3]>0);same_mat=covered&(o[:,3]==n[:,3]);same_bank=o[:,0]==n[:,0];du=np.minimum(np.abs(o[:,1]-n[:,1]),4096-np.abs(o[:,1]-n[:,1]));dv=np.minimum(np.abs(o[:,2]-n[:,2]),4096-np.abs(o[:,2]-n[:,2]));same=same_mat&same_bank&(du==0)&(dv==0)
 r=recipes[np.maximum(0,ids)];inside=np.ones(len(o),bool)
 for z in [o,n]:inside&=(((z[:,1]-(r[:,3]-2))&4095)<r[:,5]+4)&(((z[:,2]-(r[:,4]-2))&4095)<r[:,6]+4)
 interior=np.ones(len(o),bool)
 for z in [o,n]:interior&=(((z[:,1]-r[:,3])&4095)<r[:,5])&(((z[:,2]-r[:,4])&4095)<r[:,6])
 adjacent=same_mat&same_bank&inside&(du<=1)&(dv<=1)&~same;classes=np.where(same,'a',np.where(adjacent,'b','c'));coords=np.argwhere(diff);error=np.max(np.abs(a.astype(int)-b.astype(int)),axis=2)[diff]
 counts={c:int(np.count_nonzero(classes==c)) for c in ['a','b','c']};examples={c:[] for c in counts}
 for i,(y,x) in enumerate(coords):
  event={'image':name,'tick':tick,'x':int(x),'y':int(y),'class':str(classes[i]),'RGBA_bank':int(o[i,0]),'RGBA_u':int(o[i,1]),'RGBA_v':int(o[i,2]),'RGBA_material':int(o[i,3])-1,'index_bank':int(n[i,0]),'index_u':int(n[i,1]),'index_v':int(n[i,2]),'index_material':int(n[i,3])-1,'du':int(du[i]),'dv':int(dv[i]),'within_footprint_with_gutter':bool(inside[i]),'within_original_uv_bounds':bool(interior[i]),'channel_error':int(error[i])};events.append(event)
  if len(examples[str(classes[i])])<3:examples[str(classes[i])].append(event)
 rows.append({'image':name,'different_pixels':len(o),'classes':counts,'maximum_channel_error':int(error.max()) if len(error) else 0,'examples':examples})
 # Both final images must depend on the world pixel at the same coordinate.
 if '.composed.' in name:
  aw=np.fromfile(p.with_name(name.replace('.composed.','.world.')),np.uint8).reshape(480,640,4);bw=np.fromfile(base/'colour-index/images'/name.replace('.composed.','.world.'),np.uint8).reshape(480,640,4);assert not np.any(diff&~np.any(aw!=bw,axis=2))
 if '.world.' in name:
  mismatch=(old[:,:,3]!=new[:,:,3])|((old[:,:,0]!=new[:,:,0])&(old[:,:,3]>0));global_issues.append({'tick':tick,'bank_or_material_mismatches_all_world_pixels':int(mismatch.sum())})
totals={c:sum(r['classes'][c] for r in rows) for c in ['a','b','c']};result={'images':len(rows),'residual_pixels':len(events),'classes':totals,'gate_pass':totals['c']==0,'adjacent_definition':'8-connected texel adjacency (du<=1,dv<=1,nonzero), modulo4096, identical material+bank, both within original footprint plus source-aware2texel gutters','provenance_for_composed':'World contribution at the same framebuffer coordinate; every composed residual is a world residual. HUD logic unchanged. No claim HUD font pixels originate in world ROM texture.','rows':rows,'all_world_identity_checks':global_issues}
(ev/'classification.json').write_text(json.dumps(result,indent=2)+'\n')
with (ev/'pixels.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(events[0]));w.writeheader();w.writerows(events)
assert len(rows)==36 and len(events)==5875
print(json.dumps({'classes':totals,'residual_pixels':len(events),'global_identity_mismatches':sum(x['bank_or_material_mismatches_all_world_pixels'] for x in global_issues)},indent=2))
