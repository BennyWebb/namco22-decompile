#!/usr/bin/env python3
"""Prepare bounded-memory debug resources; normal C1d shader/atlas math unchanged."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3];out=root/'raverace/native/generated/c1e/axis'
for mode in ['rgba','index']:
 src=root/'raverace/native/generated/c1d'/mode;dst=out/mode;dst.mkdir(parents=True,exist_ok=True)
 for p in src.iterdir():
  if not p.is_file():continue
  q=dst/p.name
  if q.is_symlink():q.unlink()
  shutil.copy2(p,q)
 p=dst/'provenance.frag';s=p.read_text();qual='highp usampler2D' if mode=='index' else 'lowp sampler2D';s=s.replace('uniform '+qual+' coordinate_image;','uniform '+qual+' coordinate_image;uniform '+qual+' coordinate_y;')
 if mode=='index':s=s.replace('uvec4 bytes=texture(coordinate_image,texcoord);','uvec4 bytes=uvec4(texture(coordinate_image,vec2(texcoord.x,0.5)).rg,texture(coordinate_y,vec2(0.5,texcoord.y)).rg);')
 else:s=s.replace('texture(coordinate_image,texcoord)*255.0','vec4(texture(coordinate_image,vec2(texcoord.x,0.5)).rg,texture(coordinate_y,vec2(0.5,texcoord.y)).rg)*255.0')
 p.write_text(s)
print(out)
