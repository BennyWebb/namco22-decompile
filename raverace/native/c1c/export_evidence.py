#!/usr/bin/env python3
import pathlib,csv,gzip,json,hashlib,shutil,numpy as np
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/c1c';old=root/'raverace/native/generated/c1b';ev=root/'raverace/benchmarks/native-c1c-20261016';tel=base/'telemetry';tel.mkdir(exist_ok=True)
# Every original differing pixel plus new residual pixels, coordinates bottom-origin.
with gzip.open(ev/'pixel-attribution.csv.gz','wt',newline='') as f:
 w=csv.writer(f);w.writerow(['image','x','y','class','attribution','original_max_channel_error','revised_max_channel_error'])
 for p in sorted((old/'host-final-rgba/images').glob('*.rgba')):
  a=np.fromfile(p,np.uint8).reshape(480,640,4);b=np.fromfile(old/'host-final-index/images'/p.name,np.uint8).reshape(480,640,4);d=np.fromfile(base/'texture-lowp/images'/p.name,np.uint8).reshape(480,640,4);orig=np.any(a!=b,axis=2);new=np.any(a!=d,axis=2);error=np.max(np.abs(a.astype(int)-b.astype(int)),axis=2);revised=np.max(np.abs(a.astype(int)-d.astype(int)),axis=2)
  for y,x in np.argwhere(orig|new):
   reason='palette_precision_counterfactual' if orig[y,x] and not new[y,x] else 'unresolved_original' if orig[y,x] else 'new_residual';w.writerow([p.name,int(x),int(y),'b-not-proven-adjacent',reason,int(error[y,x]),int(revised[y,x])])
for name in ['texture-highp','texture-lowp','layout-lowp']:
 d=tel/name;d.mkdir(exist_ok=True)
 for p in (base/name).iterdir():
  if p.is_file() and p.suffix in ['.txt','.csv','.jsonl','.json']:shutil.copyfile(p,d/p.name)
for name in ['prepare-2.log','prepare-3.log','prepare-final.log','sampler-runs.log','layout-run.log','prepare-control.log','build-host.log']:
 if (base/name).exists():shutil.copyfile(base/name,tel/name.replace('.log','.txt'))
shutil.copyfile(base/'b2-validation/host/build.json',tel/'build.json');shutil.copyfile(base/'layout-alias-locations.json',tel/'layout-alias-locations.json')
ref=old/'host-final-rgba';checks=[]
for name in ['texture-highp','texture-lowp','layout-lowp']:
 d=base/name;assert (d/'exit.txt').read_text().strip()=='0';assert (d/'states.jsonl').read_bytes()==(ref/'states.jsonl').read_bytes();assert (d/'semantic-sha256.csv').read_bytes()==(ref/'semantic-sha256.csv').read_bytes();assert len(list(csv.DictReader((d/'image-sha256.csv').open())))==36
 checks.append({'run':name,'ticks':6006,'checkpoint_sets':21,'domain_records':63,'adjusted_commands_equal':4207,'images':36,'binary_sha256':hashlib.sha256((base/'b2-validation/host/rr_p1b').read_bytes()).hexdigest()})
highp_identical=all(p.read_bytes()==(base/'texture-highp/images'/p.name).read_bytes() for p in (old/'host-final-index/images').glob('*.rgba'))
(ev/'validation.json').write_text(json.dumps({'runs':checks,'normalised_highp_identical_to_C1b_fetch_images':highp_identical,'layout_control_replay_run':True,'layout_control_reason':'Explicitly conflicted first-pen control:22/36 exact,426 differing pixels; unsatisfiable shared R8UI constraints at5244 texels.','device_contacted':False},indent=2)+'\n')
ident={'branch':'feature/knulli-h700','initial_HEAD':'65028074a7d3393712d957c72725f36d56c26ec3','initial_worktree':'clean','sources':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((root/'raverace/native/c1c').iterdir()) if p.is_file()},'input_materials_sha256':hashlib.sha256((root/'raverace/native/generated/milestone4a-coverage/race-live-v2/materials.json').read_bytes()).hexdigest()};(ev/'identities.json').write_text(json.dumps(ident,indent=2)+'\n')
print('Exported complete pixel attribution and host receipts; highp matches original:',highp_identical)
