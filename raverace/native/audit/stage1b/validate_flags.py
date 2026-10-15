#!/usr/bin/env python3
"""Validate complete checkpoints and exact packet bytes; never performance data."""
import argparse, csv, hashlib, json, pathlib
p = argparse.ArgumentParser()
p.add_argument('run', type=pathlib.Path)
p.add_argument('reference_states', type=pathlib.Path)
p.add_argument('reference_packets', type=pathlib.Path)
a = p.parse_args()
expected = [json.loads(x) for x in a.reference_states.read_text().splitlines()]
actual = [json.loads(x) for x in (a.run/'states.jsonl').read_text().splitlines()]
assert len(expected) == 63 and actual == expected, 'Authoritative checkpoint mismatch'
assert (a.run/'exit.txt').read_text().strip() == '0', 'Replay failed'
def receipts(directory):
    result = {}
    for f in sorted(directory.glob('tick-*.live')):
        data = f.read_bytes()
        result[int(f.stem.split('-')[1])] = (len(data), hashlib.sha256(data).hexdigest())
    return result
reference = receipts(a.reference_packets)
receipt_file = a.run/'command-sha256.csv'
if receipt_file.exists():
    rows = list(csv.DictReader(receipt_file.open()))
    observed = {int(r['tick']):(int(r['bytes']),r['sha256']) for r in rows}
    assert len(observed) == len(rows), 'Duplicate packet receipt'
else:
    observed = receipts(a.run/'commands')
assert set(reference) == set(observed) == set(range(1800,6007)), 'Incomplete packets'
assert observed == reference, 'Command packet byte mismatch'
result = dict(full_replay_ticks=6006, authoritative_checkpoint_sets=21,
              authoritative_domain_records=63, all_authoritative_records_identical=True,
              exact_command_packets=4207, packet_bytes=sum(x[0] for x in reference.values()),
              all_command_bytes_identical=True,
              validation_only=True, performance_timings_admissible=False)
(a.run/'flag-validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
