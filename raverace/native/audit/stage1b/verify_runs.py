#!/usr/bin/env python3
"""Check retained game runs, including failures, without filtering samples."""
import argparse, pathlib, json
p=argparse.ArgumentParser();p.add_argument('evidence',type=pathlib.Path);p.add_argument('reference_states',type=pathlib.Path);a=p.parse_args()
expected=[json.loads(x) for x in a.reference_states.read_text().splitlines()]
out={}
for run in sorted(a.evidence.iterdir()):
    if not run.is_dir() or not (run/'states.jsonl').exists() or not (run/'restored.txt').exists():continue
    states=[json.loads(x) for x in (run/'states.jsonl').read_text().splitlines()]
    result=dict(exit=int((run/'exit.txt').read_text()),checkpoints=len(states)//3,
                through_tick=max([x.get('frames',x.get('board_frame',x.get('audio_frame',0))) for x in states],default=0),
                all_authoritative_records_identical=states==expected,
                clock_restore_identical=(run/'restoration.expected').read_bytes()==(run/'restored.txt').read_bytes(),
                production_hash_identical=(run/'production.before').read_bytes()==(run/'production.after').read_bytes(),
                emulationstation_restored=(run/'es-restored.txt').exists() and bool((run/'es-restored.txt').read_text().strip()))
    out[run.name]=result
(a.evidence/'all-run-validation.json').write_text(json.dumps(out,indent=2)+'\n')
failed={k:v for k,v in out.items() if v['exit']!=0 or not all(v[x] for x in ['all_authoritative_records_identical','clock_restore_identical','production_hash_identical','emulationstation_restored'])}
print(json.dumps(dict(runs=len(out),failed=failed),indent=2))
raise SystemExit(bool(failed))
