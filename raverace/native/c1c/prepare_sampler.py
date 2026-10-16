#!/usr/bin/env python3
import pathlib,shutil
root=pathlib.Path(__file__).resolve().parents[3];out=root/'raverace/native/generated/c1c/compact';out.mkdir(exist_ok=True)
for p in (root/'raverace/native/generated/c1b/reference').iterdir():
 if p.is_file() and p.suffix in ['.bin','.r8ui','.meta','.vert']:shutil.copyfile(p,out/p.name)
for sampler in ['texture','fetch']:
 for precision in ['highp','lowp']:
  src=(root/'raverace/native/generated/c1b/reference/index.frag').read_text().replace('uniform highp sampler2D palette;',f'uniform {precision} sampler2D palette;')
  if sampler=='texture':src=src.replace('texelFetch(image,xy,0).r','texture(image,texcoord).r')
  (out/f'index_{sampler}_{precision}.frag').write_text(src)
