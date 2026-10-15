#!/usr/bin/env python3
"""Verify source painter ranks, D16 separation and bounded opaque batch permutation."""
import argparse,pathlib,sys,json,numpy as np,hashlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'scene'));from compact_protocol import Reader
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('opacity',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();proof=json.loads(a.opacity.read_text());manifest=(a.pack/'manifest.json').read_bytes();assert hashlib.sha256(manifest).hexdigest()==proof['pack_manifest_sha256'] and proof['all_opaque'];r=Reader(a.pack/'frames.rsc');min_gap=1e9;min_bins=65536;commands=groups=protected=near=reordered=0
for tick in range(1800,5640):
 f=r.read(tick);q=f['quads'];rank=np.lexsort((-q['order'].astype(np.int64),-q['zsort'].astype(np.int64)));d=q['depth'][rank].astype(np.float64);assert np.all(np.diff(d)<0),'painter rank mismatch';assert np.all((d>-.9)&(d<.9));bins=np.rint((d*.5+.5)*65535).astype(np.int64);gap=-np.diff(bins);assert np.all(gap>8),'D16 ties or insufficient guard';min_bins=min(min_bins,int(gap.min()));min_gap=min(min_gap,float((-np.diff(d)).min()));v=f['visible'].copy()
 for first,count,page,mode,*box in f['groups']:
  first,count,mode=map(int,[first,count,mode]);items=v[first:first+count];assert len(np.unique(q['depth'][items]))==count
  if mode&2:protected+=count
  elif mode&1:near+=count
  else:
   assert np.all(np.diff(q['depth'][items].astype(np.float64))<0),'batch is not strict painter order';ordered=items[::-1];assert np.array_equal(ordered,items[np.argsort(q['depth'][items],kind='stable')]);reordered+=int(np.count_nonzero(ordered!=items));v[first:first+count]=ordered;groups+=1
 assert set(map(int,v))==set(map(int,f['visible'])) and len(v)==len(f['visible']);commands+=len(q)
out=dict(ticks=3840,source_commands=commands,pack_manifest_sha256=proof['pack_manifest_sha256'],depth_buffer_bits=16,minimum_float_depth_gap=min_gap,minimum_quantised_depth_gap=min_bins,quantised_ties=0,conservative_required_bin_guard=8,reordered_indices=reordered,reorderable_groups=groups,protected_indices_preserved=protected,near_indices_preserved=near,policy='Linear reverse equals stable ascending synthetic depth only within original ordinary unprotected opaque group; group boundaries, draws, all source IDs and protected/near order remain unchanged. D16 CPU quantisation separation is audited; driver raster/clipping precision still requires full host and Mali image checks.');a.out.write_text(json.dumps(out,indent=2)+'\n');print(out)
