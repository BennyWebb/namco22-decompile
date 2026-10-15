#!/usr/bin/env python3
"""Export actual snapshot instance/camera metadata using the incumbent geometry stage.
Private diagnostic copies only; no emulator/game source modifications.
"""
import argparse,json,pathlib,subprocess,hashlib,struct
import numpy as np
p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('snapshot',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--scene',type=pathlib.Path,required=True);a=p.parse_args()
assert json.loads((a.snapshot/'state.json').read_bytes())==json.loads((a.scene/'state.json').read_bytes()),'snapshot/frozen scene endpoint mismatch'
root=pathlib.Path(__file__).resolve().parents[3];out=a.out.resolve();assert not out.exists() and not out.is_relative_to(root);out.mkdir(parents=True)
import importlib.util
spec=importlib.util.spec_from_file_location('asset_tool',root/'raverace/native/assets/asset_tool.py');t=importlib.util.module_from_spec(spec);spec.loader.exec_module(t);rom=t.ROM(a.roms);rom.words.astype('<i4').tofile(out/'pointrom.i32')
geo=(root/'engine/geo_hw.c').read_text();assert geo.count('uint32_t uv_raw[8];')==1
geo='static unsigned native_packet_data;\n'+geo.replace('uint32_t uv_raw[8];','uint32_t uv_raw[8]; native_packet_data=addr;')
(out/'geometry.c').write_text(geo)
s=(root/'raverace/native/tools/inspect_scene_geometry.c').read_text().replace('#include "../../../engine/geo_hw.c"','#include "geometry.c"').replace('#include "../../../engine/slave_list.c"','#include '+json.dumps(str(root/'engine/slave_list.c')))
s=s.replace('if(g_bbox_cur!=wanted)return;','if(wanted>=0&&g_bbox_cur!=wanted)return;')
s=s.replace('printf("{\\"code\\":%d,','printf("{\\"source_data\\":%u,\\"code\\":%d,').replace('g_bbox_cur,q->color,q->flags_raw', 'native_packet_data,g_bbox_cur,q->color,q->flags_raw')
s=s.replace('printf("{\\"source_data\\":%u,\\"code\\":%d,','printf("{\\"source_data\\":%u,\\"zsort\\":%d,\\"clip\\":[%d,%d,%d,%d],\\"objectflags\\":%d,\\"cz_adjust\\":%d,\\"code\\":%d,')
s=s.replace('native_packet_data,g_bbox_cur,q->color','native_packet_data,q->zsort,q->clip[0],q->clip[1],q->clip[2],q->clip[3],q->objectflags,q->cz_adjust,g_bbox_cur,q->color')
(out/'inspect.c').write_text(s)
subprocess.run(['cc','-O2','-fwrapv','-I'+str(root/'engine'),str(out/'inspect.c'),'-o',str(out/'inspect'),'-lm'],check=True)
with (out/'quads.jsonl').open('w') as f:
 subprocess.run([str(out/'inspect'),str(out/'pointrom.i32'),str(a.snapshot/'poly.i32'),str(a.snapshot/'pointram.i32'),'-1'],stdout=f,check=True)
models={};checked=dynamic=valid=0;instances=set()
def trunc(n,d):return abs(n)//abs(d)*(-1 if (n<0)!=(d<0) else 1)
for line in (out/'quads.jsonl').read_text().splitlines():
 r=json.loads(line);code=r['code'];instances.add((code,tuple(r['m']),tuple(r['t']),tuple(r['zoom']),tuple(r['center'])))
 if code==5:dynamic+=1;continue
 if code not in models:
  model=json.loads((a.pack/f'model-{code}.json').read_bytes());models[code]=(model,{q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4):q for q in model['quads']})
 model,lookup=models[code];q=lookup[r['source_data']]
 assert q['color']==r['color'] and q['flags']==r['flags'] and q['material'][0]==r['bank'] and q['material'][2]==r['cmode']
 for i,actual in enumerate(r['vertices']):
  if not actual[6]:continue
  v=model['vertices'][q['first_vertex']+i];xyz=[(sum(v[k]*r['m'][k*3+c] for k in range(3))>>15)+r['t'][c] for c in range(3)]
  mant,shift=r['zoom'];cx,cy=r['center'];sat=lambda x:max(-2147483648,min(2147483647,x))
  screen=[sat(cx*16+trunc((xyz[0]*mant*16)>>shift,xyz[2])),sat(cy*16-trunc((xyz[1]*mant*16)>>shift,xyz[2]))]
  assert actual[:6]==[*screen,xyz[2],v[3],v[4],v[5]],(code,r['source_data'],i)
  valid+=1
 checked+=1
report=dict(static_source_quads=checked,dynamic_source_quads=dynamic,valid_source_vertices=valid,source_models=len(models),instances=len(instances),differences=0,scope='Display-list geometry only; direct FIFO/HUD are not exported by this inspector.')
report['native_asset_manifest_sha256']=hashlib.sha256((a.pack/'manifest.json').read_bytes()).hexdigest()
report['snapshot_poly_sha256']=hashlib.sha256((a.snapshot/'poly.i32').read_bytes()).hexdigest()
report['frozen_draws_sha256']=hashlib.sha256((a.scene/'draws.bin').read_bytes()).hexdigest()
report['same_replay_endpoint']=True
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
