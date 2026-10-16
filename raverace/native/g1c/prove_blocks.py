#!/usr/bin/env python3
"""ROM-only source-aware coalescing proof for every observed logical footprint."""
import pathlib,re,json,sys,numpy as np
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1c';sys.path.insert(0,str(root/'raverace/native/assets'));from asset_tool import ROM
rom=ROM(root/'raverace/extracted');s=(b/'attribution-clean/run.txt').read_text();keys={tuple(map(int,k.split(','))) for k in re.findall(r'G1C-KEY\] tick=\d+ key=([\d,]+)',s)};blocks={};texels=0
for bank,cm,u,v,w,h in sorted(keys):
 bu=u//256*256;bv=v//256*256;bw=((u+w+255)//256)*256-bu;bh=((v+h+255)//256)*256-bv;key=(bank,cm,bu,bv,bw,bh)
 if key not in blocks:blocks[key]=rom.pens(bank,cm,(bu-2)&4095,(bv-2)&4095,bw+4,bh+4)
 actual=blocks[key][v-bv:v-bv+h+4,u-bu:u-bu+w+4];expected=rom.pens(bank,cm,(u-2)&4095,(v-2)&4095,w+4,h+4);assert actual.shape==expected.shape and np.array_equal(actual,expected),(bank,cm,u,v,w,h);texels+=expected.size
out=dict(logical_footprints=len(keys),canonical_blocks=len(blocks),padded_block_bytes=sum(x.size for x in blocks.values()),exact_padded_logical_texels=texels,all_exact=True,rule='Round footprint interior bounds outward to ROM-space multiples of256, then add original two-source-texel gutters; affine origins reference the canonical source origin; logical recipe/group unchanged.')
(root/'raverace/benchmarks/native-g1c-20261016/block-proof.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
