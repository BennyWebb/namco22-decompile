#!/usr/bin/env python3
"""ROM/provenance authenticated immutable mesh recipe components; no frames."""
import pathlib,sys,json,struct,hashlib,argparse,numpy as np
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'assets'));from asset_tool import ROM
rom=ROM(a.roms);manifest=json.loads((a.pack/'manifest.json').read_text());assert rom.sources==manifest['source_roms'];meshes=json.loads((a.pack/'meshes.json').read_text());models={};raw=np.fromfile(a.pack/'meshes.f32',dtype='<f4').reshape(-1,4)
n=max(m['texel']//8 for m in meshes)+1;result=bytearray(n*32)
for m in meshes:
 code,packet=m['code'],m['packet']
 if code not in models:
  model=rom.model(code);models[code]={q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4):(q,np.array(model['vertices'][q['first_vertex']:q['first_vertex']+4],dtype='<i4')) for q in model['quads']}
 q,v=models[code][packet];gpu=raw[m['texel']:m['texel']+8].reshape(4,2,4);assert np.array_equal(gpu[:,0,:3],v[:,:3]) and np.array_equal(gpu[:,1,:2],v[:,3:5]);u,wv=v[:,3].min(),v[:,4].min();bank,group,mode=q['material'];struct.pack_into('<8i',result,m['texel']//8*32,code,packet,bank,mode,int(u),int(wv),int(v[:,3].max()-u+1),int(v[:,4].max()-wv+1))
a.out.mkdir(parents=True,exist_ok=True);data=struct.pack('<II',0x324d5252,n)+result;(a.out/'mesh-metadata.bin').write_bytes(data);(a.out/'metadata-proof.json').write_text(json.dumps(dict(meshes=len(meshes),entries=n,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),rom_sources=rom.sources,pack_manifest_sha256=hashlib.sha256((a.pack/'manifest.json').read_bytes()).hexdigest(),all_mesh_positions_uv_equal_rom=True,all_recipe_components_from_rom=True),indent=2)+'\n');print('Validated ROM metadata for',len(meshes),'meshes')
