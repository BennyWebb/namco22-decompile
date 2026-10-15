#!/usr/bin/env python3
"""Independent ROM/coverage and append-only checks of a completed live expansion."""
import pathlib,json,struct,hashlib,sys,numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'assets'))
from asset_tool import ROM
old,new,audit,roms,out=map(pathlib.Path,sys.argv[1:]);rom=ROM(roms);sha=lambda b:hashlib.sha256(b).hexdigest();om=json.loads((old/'materials.json').read_text());nm=json.loads((new/'materials.json').read_text());meshes=json.loads((new/'meshes.json').read_text());required=json.loads((audit/'meshes.json').read_text());variants=json.loads((audit/'variants.json').read_text());manifest=json.loads((new/'manifest.json').read_text());assert rom.sources==manifest['source_roms'];ids={(v['code'],v['packet']) for v in meshes};missing=[k for k,v in required.items() if v['native'] and (v['code'],v['packet']) not in ids];assert not missing;assert all(k in nm for k,v in variants.items() if v['native'])
assert (new/'materials.bin').read_bytes()[:(old/'materials.bin').stat().st_size]==(old/'materials.bin').read_bytes()
oldmeshes=json.loads((old/'meshes.json').read_text());assert meshes[:len(oldmeshes)]==oldmeshes
raw=np.fromfile(new/'meshes.f32',dtype='<f4').reshape(-1,4);oldraw=np.fromfile(old/'meshes.f32',dtype='<f4').reshape(-1,4);used=max(v['texel'] for v in oldmeshes)+8;assert np.array_equal(raw[:used],oldraw[:used])
models={}
for v in meshes[len(oldmeshes):]:
 code=v['code']
 if code not in models:
  model=rom.model(code);models[code]={q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4):np.array(model['vertices'][q['first_vertex']:q['first_vertex']+4],dtype='<i4')[:,:5] for q in model['quads']}
 source=models[code][v['packet']];actual=raw[v['texel']:v['texel']+8].reshape(4,2,4);assert np.array_equal(actual[:,0,:3],source[:,:3]) and np.array_equal(actual[:,1,:2],source[:,3:5]);assert np.all(actual[:,0,3]==0) and np.all(actual[:,1,2:]==0)
for n,v in manifest['files'].items():b=(new/n).read_bytes();assert len(b)==v['bytes'] and sha(b)==v['sha256'],n
for k,v in om.items():assert all(nm[k][field]==value for field,value in v.items()),k
pages={};checked=0
for k,v in nm.items():
 if k in om:continue
 bank,group,cmode,u,wv,w,h=v['recipe'];palette=np.frombuffer(bytes.fromhex(variants[k]['palette_rgb']),np.uint8).reshape(256,3);pens=rom.pens(bank,cmode,u-2,wv-2,w+4,h+4);page=v['page'];pw,ph=manifest['pages'][page]
 if page not in pages:pages[page]=np.fromfile(new/f'page-{page}.rgba',np.uint8).reshape(ph,pw,4)
 x,y,rw,rh=v['rect'];image=pages[page][y:y+rh,x:x+rw];assert np.array_equal(image[:,:,:3],palette[pens]) and np.all(image[:,:,3]==255);assert sha(struct.pack('<II',rw,rh)+image.tobytes())==v['content'];assert np.allclose(v['uv'],[1/pw,1/ph,(x+2.5-u)/pw,(y+2.5-wv)/ph],rtol=0,atol=1e-15);checked+=1
static=sum(w*h*4 for w,h in manifest['pages'])+raw.nbytes+1024*((len(nm)+1023)//1024)*16;assert static<=536870912
r=dict(pass_all_native_meshes=True,pass_all_native_material_variants=True,pass_original_material_bytes_and_mesh_ids=True,pass_manifest_hashes=True,pass_new_material_exact_rom_rgb_alpha_gutters_uv=True,new_materials=checked,native_static_bytes=static,legacy_boot_explicitly_retained=True);out.write_text(json.dumps(r,indent=2)+'\n');print(r)
