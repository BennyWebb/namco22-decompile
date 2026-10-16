#!/usr/bin/env python3
"""Host gate: no inference of native success from successful legacy fallback."""
import pathlib,json,csv,re,collections,struct,hashlib,gzip
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1';e=root/'raverace/benchmarks/native-g1-20261016';run=b/'whole-colour';ref=root/'raverace/native/generated/c1b/host-final-rgba'
states=[json.loads(l) for l in (run/'states.jsonl').read_text().splitlines()];assert states==[json.loads(l) for l in (ref/'states.jsonl').read_text().splitlines()]
rows=list(csv.DictReader((run/'thread.csv').open()));assert [int(x['frame']) for x in rows]==list(range(1,6007))
coverage=[];pending=None;reassembled=0
for l in (run/'run.txt').read_text().splitlines():
 if l.startswith('[G1-COVERAGE]'):
  if ' reason=' not in l:assert pending is None;pending=l.split('{"schema":',1)[0];continue
 elif pending and l.startswith(' direct='):
  l=pending+l;pending=None;reassembled+=1
 else:continue
 prefix,reason=l.split(' reason=',1);r={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',prefix)};r['reason']=reason;coverage.append(r)
assert not pending and len(coverage)==6006
coverage.sort(key=lambda x:x['tick']);assert [x['tick'] for x in coverage]==list(range(1,6007))
phase=[x for x in coverage if x['tick']>=1800];counts=collections.Counter(x['reason'] for x in phase);allcounts=collections.Counter(x['reason'] for x in coverage)
pack=pathlib.Path((b/'selected.txt').read_text().strip());new={tuple(m['recipe']) for m in json.loads((pack/'materials.json').read_text())};old=json.loads((root/'raverace/native/generated/milestone4a-coverage/race-live-v2/materials.json').read_text());missing=[{'old_material':m['id'],'recipe':m['recipe']} for m in old.values() if tuple(m['recipe']) not in new]
wp=(root/'raverace/native/generated/milestone4a-coverage/commands/tick-1800.live').read_bytes();h=struct.unpack_from('<12I3f',wp);off=60+h[7]*80;direct=[]
for i in range(h[3]):
 mesh,instance,mat=struct.unpack_from('<iHH',wp,off+i*32)
 if mesh<0:direct.append({'mesh':mesh,'material':mat,'absent_from_static_pack':any(x['old_material']==mat for x in missing)})
result={'status':'BLOCKED_HOST_NATIVE_PARITY','ticks':len(rows),'authoritative_checkpoint_sets':len(states)//3,'checkpoint_domain_records':len(states),'checkpoints_exact':True,'expected_adjusted_packets':4207,'actual_adjusted_packets':len(list(csv.DictReader((run/'semantic-sha256.csv').open()))),'raw_packets':len(list(csv.DictReader((run/'command-sha256.csv').open()))),'captured_images':len(list((run/'images').glob('*.rgba'))),'native_phase_pictures':len(phase),'native_phase_native':sum(x['native'] for x in phase),'native_phase_fallbacks':dict(counts),'all_replay_reasons':dict(allcounts),'unrepresented_reference_recipes':missing,'tick1800_existing_control_direct_materials':direct,'run_exit':int((run/'exit.txt').read_text()),'IO':next(x for x in (run/'run.txt').read_text().splitlines() if x.startswith('[P1B-IO]')),'device_tests_started':False,'stdout_stderr_interleaved_record_reassembly':reassembled};assert result['actual_adjusted_packets']==0;assert sum(counts.values())==4207
(e/'host-gate.json').write_text(json.dumps(result,indent=2)+'\n');(e/'coverage.json').write_text(json.dumps(coverage,indent=2)+'\n')
by=collections.Counter((x['main'],x['sub'],x['screen'],x['course'],x['native'],x['reason']) for x in coverage);(e/'coverage-by-state.json').write_text(json.dumps([dict(main=k[0],sub=k[1],screen=k[2],course=k[3],native=k[4],reason=k[5],pictures=v) for k,v in sorted(by.items())],indent=2)+'\n')
manifest=[]
for name in ['audit-state','whole-colour']:
 for p in (b/name).iterdir():
  if not p.is_file():continue
  data=p.read_bytes();out=e/'host'/name/p.name;out.parent.mkdir(parents=True,exist_ok=True)
  if len(data)>65536:out=out.with_name(out.name+'.gz');out.write_bytes(gzip.compress(data,mtime=0))
  else:out.write_bytes(data)
  manifest.append({'file':str(out.relative_to(e)),'bytes':len(data),'source_sha256':hashlib.sha256(data).hexdigest(),'stored_sha256':hashlib.sha256(out.read_bytes()).hexdigest()})
(e/'telemetry-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(result,indent=2))
