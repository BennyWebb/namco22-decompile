#!/usr/bin/env python3
import pathlib,importlib.util
root=pathlib.Path(__file__).resolve().parents[3];source=root/'raverace/native/c1c/build.py';spec=importlib.util.spec_from_file_location('prov',pathlib.Path(__file__).with_name('transform.py'));prov=importlib.util.module_from_spec(spec);spec.loader.exec_module(prov)
code=source.read_text().replace('raverace/native/generated/c1c/{variant}','raverace/native/generated/c1d/{variant}')
code=code.replace('code=source.read_text()', 'old_attribution=attribution;attribution=lambda n,s,r,a:prov.apply(n,old_attribution(n,s,r,a),r,a)\ncode=source.read_text()')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','prov':prov})
