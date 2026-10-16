#!/usr/bin/env python3
import argparse,pathlib,sys,types
import transform as p3
p=argparse.ArgumentParser();p.add_argument('build');p.add_argument('target',choices=['host','arm']);p.add_argument('step',choices=['priority','b1','b2']);p.add_argument('--validation',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];old=root/'raverace/native/p1b';sys.path.insert(0,str(old));import extend
import importlib.util
spec=importlib.util.spec_from_file_location('p2_transform',root/'raverace/native/p2/transform.py');p2=importlib.util.module_from_spec(spec);spec.loader.exec_module(p2)
original=extend.apply;options=types.SimpleNamespace(step='b',validation=a.validation,no_metadata=True,no_z_reuse=True)
extend.apply=lambda n,s,r,t:p3.apply(n,p2.apply(n,original(n,s,r,t),r,options),r,a)
code=(old/'build.py').read_text();variant=a.step+('-validation' if a.validation else '-play');code=code.replace("root/'raverace/native/generated/p1b'/a.target",f"root/'raverace/native/generated/p3/{variant}'/a.target")
sys.argv=[str(old/'build.py'),a.build,a.target];exec(compile(code,str(old/'build.py'),'exec'),{'__file__':str(old/'build.py'),'__name__':'__main__'})
