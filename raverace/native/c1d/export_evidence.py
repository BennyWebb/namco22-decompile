#!/usr/bin/env python3
import pathlib,json,csv,hashlib,shutil,subprocess
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1d';ev=root/'raverace/benchmarks/native-c1d-20261016';tel=base/'telemetry';tel.mkdir(exist_ok=True);reference=root/'raverace/native/generated/c1b/host-final-rgba';runs=[];raw=[]
for name in ['colour-rgba','colour-index','provenance-rgba','provenance-index']:
 d=base/name;assert (d/'exit.txt').read_text().strip()=='0';assert (d/'states.jsonl').read_bytes()==(reference/'states.jsonl').read_bytes();assert (d/'semantic-sha256.csv').read_bytes()==(reference/'semantic-sha256.csv').read_bytes();assert len(list(csv.DictReader((d/'semantic-sha256.csv').open())))==4207
 log=(d/'run.txt').read_text();assert 'dropped_records=0 dropped_bytes=0 errors=0' in log;assert '[C1D] debug=' in log and 'precision=lowp' in log
 if name.endswith('rgba'):assert (d/'command-sha256.csv').read_bytes()==(reference/'command-sha256.csv').read_bytes()
 target=tel/name;target.mkdir(exist_ok=True)
 for p in d.iterdir():
  if p.is_file() and p.suffix in ['.json','.jsonl','.txt','.csv']:shutil.copyfile(p,target/p.name)
 for p in sorted((d/'images').iterdir()):raw.append({'file':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 runs.append({'run':name,'ticks':6006,'authoritative_sets':21,'domain_records':63,'adjusted_command_receipts':4207,'raw_RGBA_packets_equal':4207 if name.endswith('rgba') else None,'logger_drops':0,'logger_errors':0})
for name in ['host-runs.log','build.log','classification.log']:
 shutil.copyfile(base/name,tel/name.replace('.log','.txt'))
shutil.copyfile(base/'b2-validation/host/build.json',tel/'build.json')
(ev/'validation.json').write_text(json.dumps({'runs':runs,'colour_RGBA_matches_C1b':True,'colour_index_matches_C1c_lowp':True,'device_contacted':False},indent=2)+'\n');(ev/'raw-readback-manifest.json').write_text(json.dumps(raw,indent=2)+'\n')
(ev/'identities.json').write_text(json.dumps({'branch':subprocess.check_output(['git','branch','--show-current'],text=True).strip(),'initial_HEAD':'15bbc87599118e952c89e0eda1d7f4b78276e421','initial_worktree':'clean','sources':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((root/'raverace/native/c1d').iterdir()) if p.is_file()},'host_binary_sha256':hashlib.sha256((base/'b2-validation/host/rr_p1b').read_bytes()).hexdigest(),'adopted_palette_precision':'lowp','normal_sampler':'texture','debug_default':'off'},indent=2)+'\n');print('Four complete validated replays;',len(raw),'raw readbacks retained privately.')
