#!/usr/bin/env python3
"""Validate an already completed 4A run; no simulation, rendering or deployment.

Explicit partial-checkpoint validation never reports a complete replay pass.
"""
import argparse,pathlib,sys,json,struct,numpy as np
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'))
from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('commands',type=pathlib.Path);p.add_argument('recording',type=pathlib.Path);p.add_argument('checkpoints',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--partial-through',type=int);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True);r=Reader(a.pack/'frames.rsc');commands=visible=0
for tick in range(1800,5640):
 actual=bytearray((a.commands/f'tick-{tick}.live').read_bytes());reference=r.read(tick)['raw'];h=struct.unpack_from('<12I3f',actual);assert h[2]==tick;commands+=h[3];visible+=h[4];actual[32:44]=reference[32:44];assert actual==reference,('live command mismatch',tick)
source=[json.loads(x) for x in pathlib.Path(str(a.recording)+'.states.jsonl').read_text().splitlines()];observed=[json.loads(x) for x in a.checkpoints.read_text().splitlines()]
def key(x):return (x.get('frames',x.get('board_frame',x.get('audio_frame'))),'simulation' if 'frames'in x else 'board' if 'board_frame'in x else 'audio')
expected={key(x):x for x in source if a.partial_through is None or key(x)[0]<=a.partial_through};got={key(x):x for x in observed};assert len(got)==len(observed);assert expected.keys()==got.keys(), 'Missing/extra checkpoint domains';assert all(v==got[k] for k,v in expected.items()),'State/board/audio divergence';assert len(expected)%3==0
result=dict(racing_window_frames=3840,source_commands=commands,visible_commands=visible,normalized_command_bytes_identical=True,checkpoint_ticks=len(expected)//3,last_checkpoint=max(k[0] for k in expected),all_authoritative_domain_records_identical=True,partial_validation=a.partial_through is not None,full_replay_pass=a.partial_through is None,limits='Offline font/style/palette IDs only are normalised. Partial validation is explicit and cannot satisfy the complete replay gate.')
(a.out/'reproduced-validation.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
