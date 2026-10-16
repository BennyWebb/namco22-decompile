#!/usr/bin/env python3
"""ROM-only whole-game static pack; cache key covers ROMs and builder version."""
import pathlib,sys,json,struct,hashlib,time,math,importlib.util,shutil
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'raverace/native/assets'))
from asset_tool import ROM,validate_model
spec=importlib.util.spec_from_file_location('atlas',root/'raverace/native/c1b/size_atlas.py');atlas=importlib.util.module_from_spec(spec);spec.loader.exec_module(atlas)
encode=lambda x:(json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
t=time.monotonic();rom=ROM(root/'raverace/extracted');key=hashlib.sha256(encode({'sources':rom.sources,'format':'g1-static-1','builder_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()})).hexdigest();base=root/'raverace/native/generated/g1';base.mkdir(exist_ok=True);out=base/('rom-'+key)
if (out/'manifest.json').exists():
 m=json.loads((out/'manifest.json').read_text())
 for n,v in m['files'].items():assert hashlib.sha256((out/n).read_bytes()).hexdigest()==v['sha256'],n
 print('CACHE',out,flush=True);sys.exit(0)
assert not out.exists(),'incomplete cache; retain and use a new builder version'
models=[];recipes=set();geometry=[];meshes=[];inventory=[];nexttexel=0
for code in range(69,2490):
 if rom.word(code)<0:continue
 m=rom.model(code);validate_model(m);models.append(code)
 for q in m['quads']:
  verts=np.asarray(m['vertices'][q['first_vertex']:q['first_vertex']+4],dtype=np.int32);u=verts[:,3];v=verts[:,4];b,g,c=q['material'];r=(b,g,c,int(u.min()),int(v.min()),int(u.max()-u.min()+1),int(v.max()-v.min()+1));recipes.add(r)
  data=np.zeros((8,4),'<f4');data[::2,:3]=verts[:,:3];data[1::2,:2]=verts[:,3:5];geometry.append(data);packet=q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4);meshes.append({'code':code,'packet':packet,'texel':nexttexel});nexttexel+=8
  inventory.append({'code':code,'packet':packet,'recipe':r,'lighting_packets':q['lighting_packet_count']})
assert len(models)==2421 and len(meshes)==97116
out.mkdir();sizing,mapping,pages=atlas.build(rom,{(r[0],*r[2:]) for r in recipes},out,'whole-ROM')
mp={tuple(m['recipe']):m for m in mapping};dims=sizing['page_dimensions'];height=math.ceil(nexttexel/1024);geo=np.zeros((height*1024,4),'<f4');geo[:nexttexel]=np.concatenate(geometry);(out/'meshes.f32').write_bytes(geo.tobytes());(out/'meshes.json').write_bytes(encode(meshes))
materials=[];matbytes=bytearray();groups=bytearray();lookup=[];meta=bytearray(1024*math.ceil(len(recipes)/1024)*32)
for i,r in enumerate(sorted(recipes)):
 b,g,c,u,v,w,h=r;m=mp[(b,c,u,v,w,h)];page=m['page'];x,y,_,_=m['rect'];pw,ph=dims[page];uv=[1/pw,1/ph,(x+2.5-u)/pw,(y+2.5-v)/ph];materials.append({'id':i,'recipe':r,'page':page,'rect':m['rect'],'uv':uv});matbytes+=struct.pack('<4fI12x',*uv,page);groups+=struct.pack('<I',g);lookup.append(struct.pack('<8i',*r,i)+bytes(768));struct.pack_into('<8i',meta,i*32,b,u,v,x+2,y+2,w,h,g)
assert len(materials)<=65535
(out/'materials.json').write_bytes(encode(materials));(out/'materials.bin').write_bytes(matbytes);(out/'groups.bin').write_bytes(groups);(out/'provenance-mapping.i32').write_bytes(meta)
(out/'live.lookup').write_bytes(struct.pack('<4I',0x344c5252,1,len(meshes),len(materials))+b''.join(struct.pack('<3i',m['code'],m['packet'],m['texel']) for m in sorted(meshes,key=lambda m:(m['code'],m['packet'])))+b''.join(lookup))
(out/'atlas.meta').write_text(str(len(dims))+'\n'+''.join(f'{w} {h}\n' for w,h in dims));(out/'race.meta').write_text(f'RRRACE3 {len(dims)} 1024 {height} {len(materials)} 1 166\n'+''.join(f'{w} {h}\n' for w,h in dims))
# Shader source comes from private source-controlled templates, never scene data.
shader=root/'raverace/native/generated/c1e/axis/index'
for name in ['race_v3.vert','race_v3_near.vert','provenance_race_v3.vert','provenance_race_v3_near.vert','index_texture_lowp.frag','index_fetch_lowp.frag','provenance.frag']:
 shutil.copyfile(shader/name,out/name)
report={'format':'g1-static-1','rom_key':key,'sources':rom.sources,'models':len(models),'meshes':len(meshes),'materials':len(materials),'atlas':sizing,'geometry_bytes':geo.nbytes,'material_bytes':len(matbytes),'build_seconds':time.monotonic()-t,'recording_inputs':0,'lighting_packet_references':sum(x['lighting_packets']>0 for x in inventory),'unresolved_static_models':0}
(out/'inventory.json').write_bytes(encode(inventory));report['files']={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.iterdir() if p.is_file()};(out/'manifest.json').write_bytes(encode(report));(base/'selected.txt').write_text(str(out)+'\n');(root/'raverace/benchmarks/native-g1-20261016/assets.json').write_bytes(encode(report));print('BUILT',out,flush=True)
