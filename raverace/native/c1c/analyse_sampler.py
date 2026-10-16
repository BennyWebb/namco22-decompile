#!/usr/bin/env python3
"""Exact per-pixel counterfactual attribution, without labelling unproven neighbours."""
import pathlib,json,hashlib,struct,zlib,numpy as np
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1c';old=root/'raverace/native/generated/c1b';ev=root/'raverace/benchmarks/native-c1c-20261016';review=base/'review';review.mkdir(exist_ok=True)
def png(p,a):
 h,w,_=a.shape
 def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
 p.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+row.tobytes() for row in a)))+chunk(b'IEND',b''))
rows=[];man=[]
for p in sorted((old/'host-final-rgba/images').glob('*.rgba')):
 paths=[p,old/'host-final-index/images'/p.name,base/'texture-highp/images'/p.name,base/'texture-lowp/images'/p.name];a,b,c,d=[np.fromfile(x,np.uint8).reshape(480,640,4) for x in paths];original=np.any(a!=b,axis=2);norm=np.any(a!=c,axis=2);low=np.any(a!=d,axis=2)
 # Disjoint ORIGINAL-difference partition. These are observed counterfactual
 # removals, not assertions of same source texel / adjacent texel equivalence.
 sampler=original&~norm;precision=original&norm&~low;unresolved=original&norm&low;assert np.array_equal(original,sampler|precision|unresolved)
 masks={'original_difference':original,'normalised_highp_difference':norm,'normalised_lowp_difference':low,'removed_by_normalised_sampler':sampler,'removed_after_lowp_precision':precision,'remaining_original_unclassified':unresolved,'new_after_changes':~original&low}
 counts={k:int(v.sum()) for k,v in masks.items()};loc={k:np.argwhere(v)[:,::-1] for k,v in masks.items()};np.savez_compressed(review/(p.stem+'.locations.npz'),**loc)
 per={}
 for k,m in masks.items():
  diff=np.abs(a.astype(np.int16)-(d if k in ['normalised_lowp_difference','remaining_original_unclassified','new_after_changes'] else c if k=='normalised_highp_difference' else b).astype(np.int16));per[k]={'pixels':int(m.sum()),'max_channel_difference':int(diff[m].max()) if np.any(m) else 0,'example_xy':loc[k][:10].tolist()}
 row={'image':p.name,'counts':counts,'classes':per,'original_max_difference':int(np.abs(a.astype(int)-b.astype(int)).max()),'normalised_max_difference':int(np.abs(a.astype(int)-c.astype(int)).max()),'lowp_max_difference':int(np.abs(a.astype(int)-d.astype(int)).max()),'alpha_differences':[int(np.count_nonzero(a[:,:,3]!=v[:,:,3])) for v in [b,c,d]]};rows.append(row)
 # All36 revised side-by-sides, plus translucent difference overlay.
 heat=np.zeros_like(a[:,:,:3]);heat[low]=[255,0,255];overlay=d[:,:,:3].copy();overlay[low]=((overlay[low].astype(np.uint16)+[255,0,255])//2).astype(np.uint8)
 for suffix,im in [('comparison',np.concatenate([a[:,:,:3],d[:,:,:3],heat],axis=1)),('overlay',overlay)]:
  out=review/(p.stem+'.'+suffix+'.png');png(out,im[::-1]);man.append({'file':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'bytes':out.stat().st_size})
 (review/(p.stem+'.original-locations.json')).write_text(json.dumps({'original':int(original.sum()),'note':'All locations in paired .locations.npz; framebuffer bottom-origin coordinates.'})+'\n')
totals={k:sum(r['counts'][k] for r in rows) for k in rows[0]['counts']};result={'images':36,'totals':totals,'rows':rows,'boundary_class_a_proven':0,'classification_complete':False,'reason':'Exact-layout control unsatisfiable due RGBA colour dedup across incompatible pen palettes; no adjacent-source GPU provenance proof performed beyond the failed control gate. Counterfactual removals are not proof of class(a).'};(ev/'sampler-analysis.json').write_text(json.dumps(result,indent=2)+'\n');(ev/'review-manifest.json').write_text(json.dumps(man,indent=2)+'\n');print(json.dumps(totals,indent=2))
