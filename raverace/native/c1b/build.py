#!/usr/bin/env python3
"""Private C1b derivation; P3 b2 math and existing game sources stay intact."""
import argparse,pathlib,sys,types,importlib.util
p=argparse.ArgumentParser();p.add_argument('build');p.add_argument('target',choices=['host','arm']);p.add_argument('--validation',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'raverace/native/p3'));import transform as p3
spec=importlib.util.spec_from_file_location('c1b_transform',pathlib.Path(__file__).with_name('transform.py'));c1=importlib.util.module_from_spec(spec);spec.loader.exec_module(c1)
old=p3.apply;p3.apply=lambda n,s,r,opts:c1.apply(n,old(n,s,r,opts),r,opts)
code=(root/'raverace/native/p3/build.py').read_text().replace('raverace/native/generated/p3/{variant}','raverace/native/generated/c1b/{variant}')
sys.argv=['build.py',a.build,a.target,'b2']+(['--validation'] if a.validation else []);exec(compile(code,str(root/'raverace/native/p3/build.py'),'exec'),{'__file__':str(root/'raverace/native/p3/build.py'),'__name__':'__main__'})
