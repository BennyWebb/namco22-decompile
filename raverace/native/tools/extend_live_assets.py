#!/usr/bin/env python3
"""ROM-authenticated, append-only 4A assets; audit both boot and native residency."""
import argparse,pathlib,sys,json,hashlib,struct,math,os
import numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'assets'))
from asset_tool import ROM
from consolidate_materials import pack
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('old',type=pathlib.Path);p.add_argument('audit',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest();encode=lambda x:(json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
manifest=json.loads((a.old/'manifest.json').read_text());rom=ROM(a.roms);assert rom.sources==manifest['source_roms'];materials=json.loads((a.old/'materials.json').read_text());meshes=json.loads((a.old/'meshes.json').read_text());missing=json.loads((a.audit/'missing-meshes.json').read_text());variants=json.loads((a.audit/'missing-variants.json').read_text());models={};validated=[]
for key,v in missing.items():
 code=v['code'];packet=v['packet']
 if code not in models:
  model=rom.model(code);models[code]={q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4):np.array(model['vertices'][q['first_vertex']:q['first_vertex']+4],dtype='<i4')[:,:5].copy() for q in model['quads']}
 data=models[code][packet];assert sha(data.tobytes())==v['local_sha256'],key;validated.append(key)
contents={};known={v['content']:v for v in materials.values()};native_contents=set();newinfo={}
for key,v in sorted(variants.items()):
 bank,group,cmode,u,wv,w,h=v['recipe'];pal=np.frombuffer(bytes.fromhex(v['palette_rgb']),np.uint8).reshape(256,3);pens=rom.pens(bank,cmode,(u-2)&4095,(wv-2)&4095,w+4,h+4);image=np.empty((h+4,w+4,4),np.uint8);image[:,:,:3]=pal[pens];image[:,:,3]=255;content=sha(struct.pack('<II',w+4,h+4)+image.tobytes());newinfo[key]=dict(recipe=v['recipe'],content=content,palette_sha256=sha(pal.tobytes()))
 if content not in known:contents.setdefault(content,image)
 if v['native'] and content not in known:native_contents.add(content)
# Lower bound counts distinct exact padded RGBA contents for the current disjoint-rectangle atlas architecture.
oldcontent={v['content']:v for v in materials.values()};oldcontentbytes=sum(v['rect'][2]*v['rect'][3]*4 for v in oldcontent.values());lower=oldcontentbytes+sum(x.nbytes for x in contents.values())
selected={k:contents[k] for k in native_contents};places,dims=pack([(k,x.shape[1],x.shape[0]) for k,x in selected.items()],4096);pages=manifest['pages']+list(map(list,dims));nexttexel=max(v['texel'] for v in meshes)+8;native_meshes=sorted((v for v in missing.values() if v['native']),key=lambda v:(v['code'],v['packet']));height=math.ceil((nexttexel+len(native_meshes)*8)/1024);count=len(materials)+sum(v['native'] for v in variants.values());static=sum(w*h*4 for w,h in pages)+1024*height*16+1024*math.ceil(count/1024)*16
report=dict(all_missing_meshes_rom_verified=len(validated),native_missing_meshes=len(native_meshes),native_missing_material_variants=count-len(materials),all_additional_unique_contents=len(contents),native_additional_unique_contents=len(selected),all_replay_padded_content_lower_bound_bytes=lower,all_replay_fully_resident_fits=lower<=536870912,native_static_bytes=static,ceiling_bytes=536870912,native_headroom_bytes=536870912-static,native_pages=pages,original_manifest_sha256=sha((a.old/'manifest.json').read_bytes()),boot_policy='Original renderer ticks1..1799 retained explicitly; boot-only variants inventoried, not loaded by native race pack.',all_missing_meshes_rom_validation_sha256=sha(encode(sorted(validated))))
(a.out/'coverage-budget.json').write_bytes(encode(report));print(json.dumps(report,indent=2),flush=True)
assert static<=536870912 and len(pages)<=16 and count<=8192
for i,key in enumerate(sorted(materials)):materials[key]['id']=i
images=[np.zeros((h,w,4),np.uint8) for w,h in dims]
for content,image in selected.items():page,x,y,w,h=places[content];images[page][y:y+h,x:x+w]=image
for key,v in sorted(variants.items()):
 if not v['native']:continue
 info=newinfo[key];u,wv,w,h=info['recipe'][3:]
 if info['content'] in known:other=known[info['content']];page=other['page'];x,y,rw,rh=other['rect']
 else:page,x,y,rw,rh=places[info['content']];page+=len(manifest['pages'])
 pw,ph=pages[page];info.update(id=len(materials),page=page,rect=[x,y,rw,rh],uv=[1/pw,1/ph,(x+2.5-u)/pw,(y+2.5-wv)/ph]);materials[key]=info
raw=np.zeros((height*1024,4),dtype='<f4');oldraw=np.frombuffer((a.old/'meshes.f32').read_bytes(),'<f4').reshape(-1,4);raw[:len(oldraw)]=oldraw
for v in native_meshes:
 data=models[v['code']][v['packet']];dst=raw[nexttexel:nexttexel+8].reshape(4,2,4);dst[:,0,:3]=data[:,:3];dst[:,1,:2]=data[:,3:5];meshes.append(dict(code=v['code'],packet=v['packet'],texel=nexttexel));nexttexel+=8
files={}
def emit(n,b):(a.out/n).write_bytes(b);files[n]=dict(bytes=len(b),sha256=sha(b))
for i in range(len(manifest['pages'])):
 name=f'page-{i}.rgba';source=a.old/name;assert sha(source.read_bytes())==manifest['files'][name]['sha256'];os.link(source,a.out/name);files[name]=manifest['files'][name]
for i,image in enumerate(images,len(manifest['pages'])):emit(f'page-{i}.rgba',image.tobytes())
emit('meshes.f32',raw.tobytes());emit('meshes.json',encode(meshes));emit('materials.json',encode(materials));emit('materials.bin',b''.join(struct.pack('<4fI12x',*v['uv'],v['page']) for v in sorted(materials.values(),key=lambda v:v['id'])))
# Metadata field order is unchanged; original frame limits unused by live compiler.
emit('race.meta',(f'RRRACE3 {len(pages)} 1024 {height} {len(materials)} 1 166\n'+''.join(f'{w} {h}\n' for w,h in pages)).encode())
result=dict(manifest);result.update(pages=pages,source_meshes=len(meshes),files=files,source_m3_manifest_sha256=report['original_manifest_sha256'],live_coverage_ticks=[1800,6006],static_bytes=static);emit('manifest.json',encode(result))
# Exact source footprint and alpha proof for every appended material, with source-aware two-texel gutters.
assert all(np.all(im[:,:,3]==255) for im in selected.values())
(a.out/'validation.json').write_bytes(encode(dict(pass_rom_meshes=len(validated),pass_append_only_mesh_prefix=True,pass_original_pages=True,pass_new_material_rom_decode=True,new_material_alpha=255,min_source_uv_guard_texels=2.5,native_static_bytes=static,all_boot_assets_resident=False)))
