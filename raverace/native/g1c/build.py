#!/usr/bin/env python3
import pathlib,sys,importlib.util
observe='--observe-cache' in sys.argv
if observe:sys.argv.remove('--observe-cache')
root=pathlib.Path(__file__).resolve().parents[3];spec=importlib.util.spec_from_file_location('g1b',pathlib.Path(__file__).with_name('observe_transform.py' if observe else 'transform.py'));g1b=importlib.util.module_from_spec(spec);spec.loader.exec_module(g1b)
src=root/'raverace/native/g1/build.py'
def derive(n,s,r,a):
 if n=='live.c' and not a.validation:
  s=s.replace('s->native_compile_ms=live_compile_ms;s->native_tag=1;','s->native_compile_ms=live_compile_ms;rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);s->native_tag=1;')
 s=g1b.apply(n,derive.old(n,s,r,a),r,a)
 if n=='live.c' and not a.validation:s=s.replace('if(s->frame>=1800)rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);','')
 return s
code=src.read_text().replace("src=root/'raverace/native/c1e/build.py';", "old_g1=g1.apply;g1.apply=derive;derive.old=old_g1;src=root/'raverace/native/c1e/build.py';").replace('generated/g1/{variant}','generated/g1c/{variant}')
if observe:code=code.replace('generated/g1c/{variant}','generated/g1c-observe/{variant}')
exec(compile(code,str(src),'exec'),{'__file__':str(src),'__name__':'__main__','g1b':g1b,'derive':derive})
