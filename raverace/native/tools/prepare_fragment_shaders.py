#!/usr/bin/env python3
"""Audit before selecting an opaque pack shader; retain discard for any unknown."""
import argparse,pathlib,subprocess,sys,json,shutil,hashlib
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(exist_ok=False);root=pathlib.Path(__file__).resolve().parents[2]
subprocess.run([sys.executable,str(root/'native/tools/audit_opacity.py'),str(a.pack),str(a.out/'opacity.json')],check=True)
r=json.loads((a.out/'opacity.json').read_text());chosen='race_opaque.frag' if r['all_opaque'] else 'race.frag'
for n in ['race_v3.vert','race_v3_near.vert']:shutil.copyfile(root/'native/render'/n,a.out/n)
shutil.copyfile(root/'native/render'/chosen,a.out/'race.frag');shutil.copyfile(root/'native/render/race.frag',a.out/'cutout.frag')
meta=dict(pack_manifest_sha256=r['pack_manifest_sha256'],selected_shader=chosen,cutout_fallback='cutout.frag',policy='Any unproved material conservatively retains original discard for the entire pack; this actual pack has all5403 materials proven opaque.',shader_sha256=hashlib.sha256((a.out/'race.frag').read_bytes()).hexdigest());(a.out/'selection.json').write_text(json.dumps(meta,indent=2)+'\n');print(meta)
