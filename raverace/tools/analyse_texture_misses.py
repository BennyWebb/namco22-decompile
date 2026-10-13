#!/usr/bin/env python3
"""Classify a diagnostic miss stream, with exact-key/coherent-palette history.
No cache-hit stream: 'reload' means decoded previously, not proof of eviction.
Usage: analyse_texture_misses.py MISSES.csv [FIRST LAST]
"""
import collections,csv,json,pathlib,sys
path=pathlib.Path(sys.argv[1]);first,last=map(int,sys.argv[2:] or [1800,5639])
seen=set();palette_history=collections.defaultdict(set)
counts=collections.Counter();texels=collections.Counter();frames=collections.defaultdict(collections.Counter)
examples=[]
key_fields=('min_u','min_v','range_u','range_v','texbank','palette','cmode','opaque','cap','used_w','used_h','alloc_w','alloc_h','decoded_w','decoded_h')
for r in csv.DictReader(path.open()):
 frame=int(r['frame']);base=tuple(r[k] for k in key_fields);pixels=base+(r['palette_hash'],)
 if pixels in seen:kind='repeat_same_pixels'
 elif base in palette_history:kind='previous_geometry_new_palette'
 else:kind='first_geometry_key'
 seen.add(pixels);palette_history[base].add(r['palette_hash'])
 if first<=frame<=last:
  n=int(r['decoded_w'])*int(r['decoded_h']);counts[kind]+=1;texels[kind]+=n;frames[frame][kind]+=1
  if 1960<=frame<=1975:examples.append(dict(r,classification=kind))
report={'first':first,'last':last,'counts':dict(counts),'decoded_texels':dict(texels),
 'frames':{str(k):dict(v) for k,v in frames.items()},'first_hitch':examples,
 'limitation':'Tracks prior misses, not cache hits or actual eviction events. Palette hashes are diagnostic identifiers, never correctness decisions.'}
output=path.with_suffix('.analysis.json');output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'counts':report['counts'],'decoded_texels':report['decoded_texels'],
 'tick1965':report['frames'].get('1965',{})},indent=2));print(output)
