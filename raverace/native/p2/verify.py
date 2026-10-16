#!/usr/bin/env python3
"""Exact P2 validation build receipts, authoritative checkpoints and image hashes."""
import pathlib,json,csv,argparse
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--images',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];ref=root/'raverace/benchmarks/native-p1-20261015/validation-q3';b=a.run
assert (b/'exit.txt').read_text().strip()=='0';assert [json.loads(x) for x in (b/'states.jsonl').read_text().splitlines()]==[json.loads(x) for x in (ref/'states.jsonl').read_text().splitlines()]
def read(p):
 rows=list(csv.DictReader(p.open()));assert len(rows)==4207 and [int(r['tick']) for r in rows]==list(range(1800,6007));return rows
assert read(b/'command-sha256.csv')==read(ref/'command-sha256.csv')
if a.images:
 assert len(list(csv.DictReader((b/'image-sha256.csv').open())))==36
 assert (b/'image-sha256.csv').read_bytes()==a.images.read_bytes()
result=dict(full_ticks=6006,authoritative_sets=21,domain_records=63,exact_native_packets=4207,images_equal=36 if a.images else None,performance_timings_admissible=False);(b/'parity.json').write_text(json.dumps(result,indent=2)+'\n');print(b.name,'PASS',result)
