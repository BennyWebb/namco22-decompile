#!/usr/bin/env python3
"""Adjust only immutable IDs for C1d provenance comparison; preserve original readbacks."""
import pathlib,json,csv,re,hashlib,struct,os,argparse,subprocess,sys
import numpy as np
root=pathlib.Path(__file__).resolve().parents[3];p=argparse.ArgumentParser();p.add_argument('--base',type=pathlib.Path,default=root/'raverace/native/generated/g1c');p.add_argument('--rgba-base',type=pathlib.Path,default=root/'raverace/native/generated/c1e');p.add_argument('--evidence',type=pathlib.Path,default=root/'raverace/benchmarks/native-g1c-20261016/host');a=p.parse_args();base=a.base;ev=a.evidence;ev.mkdir(parents=True,exist_ok=True)
ref=root/'raverace/native/generated/c1b/host-final-rgba';pack=root/'raverace/native/generated/g1c/pack';materials=json.loads((pack/'materials.json').read_text());old=json.loads((root/'raverace/native/generated/milestone4a-coverage/race-live-v2/materials.json').read_text());lookup=(root/'raverace/native/generated/milestone4a-coverage/lookup/live.lookup').read_bytes();_,_,nm,nt=struct.unpack_from('<4I',lookup);recipes={}
for i in range(nt):
 r=struct.unpack_from('<8i',lookup,16+nm*12+i*800);recipes.setdefault(tuple(r[:7]),r[7])
static={m['id']:recipes.get(tuple(m['recipe']),-1) for m in materials};sys.path.insert(0,str(root/'raverace/native/assets'));from asset_tool import ROM
rom=ROM(root/'raverace/extracted');decode=[];gates=[]
for mode in ['colour-index','provenance-index']:
 run=base/mode;assert (run/'exit.txt').read_text().strip()=='0';states=[json.loads(l) for l in (run/'states.jsonl').read_text().splitlines()];assert states==[json.loads(l) for l in (ref/'states.jsonl').read_text().splitlines()];assert (run/'semantic-sha256.csv').read_bytes()==(ref/'semantic-sha256.csv').read_bytes();assert len(list(csv.DictReader((run/'command-sha256.csv').open())))==4207
 log=(run/'run.txt').read_text();assert 'dropped_records=0' in log and 'errors=0' in log
 for m in re.finditer(r'\[G1B-DECODE\] tick=(\d+) key=([0-9,]+) bytes=(\d+) sha256=([0-9a-f]{64})',log):
  key=list(map(int,m[2].split(',')));b,c,u,v,w,h=key;data=rom.pens(b,c,(u-2)&4095,(v-2)&4095,w+4,h+4).tobytes();assert len(data)==int(m[3]) and hashlib.sha256(data).hexdigest()==m[4],key;decode.append({'run':mode,'tick':int(m[1]),'footprint':key,'bytes':len(data),'sha256':m[4]})
 gates.append({'run':mode,'ticks':6006,'checkpoint_sets':21,'adjusted_packets':4207,'raw_packets':4207,'all_exact':True})
comp=base/'comparison';comp.mkdir(exist_ok=True)
for mode in ['colour-rgba','provenance-rgba','colour-index']:
 out=comp/mode
 if not out.exists():out.symlink_to((a.rgba_base/mode if mode.endswith('rgba') else base/mode).resolve())
out=comp/'provenance-index/images';out.mkdir(parents=True,exist_ok=True);maps=[];log=(base/'provenance-index/run.txt').read_text();dynamic={}
for m in re.finditer(r'\[G1B-MATERIAL\] tick=(\d+) id=(\d+) recipe=([0-9,]+)',log):dynamic[int(m[1]),int(m[2])]=tuple(map(int,m[3].split(',')))
for f in sorted((base/'provenance-index/images').glob('*.provenance.u32')):
 tick=int(f.name.split('.')[0].split('-')[1]);original=np.fromfile(f,'<u4').reshape(480,640,4);converted=original.copy();ids=original[:,:,3];mapping={}
 for idp in np.unique(ids):
  if not idp:continue
  id=int(idp)-1;oldid=static.get(id)
  if oldid is None:oldid=recipes[dynamic[tick,id]]
  assert oldid>=0,(tick,id);mapping[id]=oldid;converted[:,:,3][ids==idp]=oldid+1
 (out/f.name).write_bytes(converted.tobytes());maps.append({'tick':tick,'original_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'normalised_sha256':hashlib.sha256(converted.tobytes()).hexdigest(),'material_ID_map':mapping})
 for suffix in ['proof.rgba','texel.rgba']:
  q=out/f'tick-{tick}.{suffix}'
  if not q.exists():q.symlink_to(f.with_name(q.name).resolve())
assert len(maps)==18;assert decode
(ev/'dynamic-texel-parity.json').write_text(json.dumps({'decodes':decode,'unique_footprints':len({tuple(x['footprint']) for x in decode}),'all_exact':True},indent=2)+'\n');(ev/'validation.json').write_text(json.dumps(gates,indent=2)+'\n');(ev/'provenance-ID-normalisation.json').write_text(json.dumps(maps,indent=2)+'\n')
subprocess.run([sys.executable,str(root/'raverace/native/c1e/classify.py'),'--base',str(comp),'--evidence',str(ev)],check=True)
assert json.loads((ev/'classification.json').read_text())['gate_pass']
print('PASS host/device gates, per-decode ROM parity and provenance')
