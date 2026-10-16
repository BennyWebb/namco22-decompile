#!/usr/bin/env python3
import pathlib,sys,importlib.util
root=pathlib.Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('g1',pathlib.Path(__file__).with_name('transform.py'));g1=importlib.util.module_from_spec(spec);spec.loader.exec_module(g1)
src=root/'raverace/native/c1e/build.py';code=src.read_text().replace("extra.COMPACT_FENCE=args.compact_fence", "extra.COMPACT_FENCE=args.compact_fence;original_extra=extra.apply;extra.apply=lambda n,s,r,a:g1.apply(n,original_extra(n,s,r,a),r,a)").replace('generated/c1e/{variant}','generated/g1/{variant}')
sys.argv=[str(src),*sys.argv[1:]];exec(compile(code,str(src),'exec'),{'__file__':str(src),'__name__':'__main__','g1':g1})
