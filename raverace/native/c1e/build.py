#!/usr/bin/env python3
"""Isolated C1e host/ARM validation and play relinks; no original objects changed."""
import argparse,pathlib,importlib.util
p=argparse.ArgumentParser();p.add_argument('target',choices=['host','arm']);p.add_argument('--validation',action='store_true');p.add_argument('--compact-fence',action='store_true');args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3]
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
prov=module('prov',root/'raverace/native/c1d/transform.py');extra=module('extra',pathlib.Path(__file__).with_name('transform.py'));source=root/'raverace/native/c1c/build.py';extra.COMPACT_FENCE=args.compact_fence
code=source.read_text().replace('raverace/native/generated/c1c/{variant}','raverace/native/generated/c1e/{variant}');code=code.replace('code=source.read_text()', 'old_attribution=attribution;attribution=lambda n,s,r,a:extra.apply(n,prov.apply(n,old_attribution(n,s,r,a),r,a),r,a)\ncode=source.read_text()')
build='/tmp/rr-m3-reference-gl' if args.target=='host' else str(root/'raverace/build/playable-knulli')
code=code.replace("sys.argv=[str(source),'/tmp/rr-m3-reference-gl','host','--validation']",'sys.argv='+repr([str(root/'raverace/native/c1b/build.py'),build,args.target]+(['--validation'] if args.validation else [])))
if args.compact_fence:code=code.replace('generated/c1e/{variant}','generated/c1e-fence/{variant}')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','prov':prov,'extra':extra})
