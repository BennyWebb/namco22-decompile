#!/usr/bin/env python3
"""P2 isolated derivation of unchanged P1b; separate validation/play binaries."""
import argparse,pathlib,sys
import transform
p=argparse.ArgumentParser();p.add_argument('build');p.add_argument('target',choices=['host','arm']);p.add_argument('step',choices=['play','a','b','c','d','e']);p.add_argument('--validation',action='store_true');p.add_argument('--profile',action='store_true');p.add_argument('--no-metadata',action='store_true');p.add_argument('--no-z-reuse',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[3];old=root/'raverace/native/p1b';sys.path.insert(0,str(old));import extend
original=extend.apply
extend.apply=lambda n,s,r,t:transform.apply(n,original(n,s,r,t),r,a)
code=(old/'build.py').read_text();variant=a.step+('-validation' if a.validation else '-play')+('-profile' if a.profile else '')+('-no-metadata' if a.no_metadata else '')+('-no-z-reuse' if a.no_z_reuse else '')
code=code.replace("root/'raverace/native/generated/p1b'/a.target",f"root/'raverace/native/generated/p2/{variant}'/a.target")
if a.profile:code=code.replace("*get('C_FLAGS'),'-c'","*get('C_FLAGS'),'-g','-fno-omit-frame-pointer','-fno-inline','-c'")
sys.argv=[str(old/'build.py'),a.build,a.target]
exec(compile(code,str(old/'build.py'),'exec'),{'__file__':str(old/'build.py'),'__name__':'__main__'})
