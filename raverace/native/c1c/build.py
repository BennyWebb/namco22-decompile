#!/usr/bin/env python3
"""Host-only sampler attribution, leaving C1b and production unchanged."""
import pathlib,sys
root=pathlib.Path(__file__).resolve().parents[3];source=root/'raverace/native/c1b/build.py'
def attribution(name,s,r,a):
 if name!='live.c':return s
 s=s.replace('static bool c1_index;', 'static char c1_fragment[80];static bool c1_index;')
 old=' const char*c1dir=getenv("ENG_C1_ATLAS");'
 new=''' const char*sampler=getenv("ENG_C1_SAMPLER");if(!sampler)sampler="texture";CHECK3(!strcmp(sampler,"texture")||!strcmp(sampler,"fetch"));
 const char*prec=getenv("ENG_C1_PALETTE_PRECISION");if(!prec)prec="highp";CHECK3(!strcmp(prec,"highp")||!strcmp(prec,"lowp"));
 snprintf(c1_fragment,sizeof c1_fragment,"index_%s_%s.frag",sampler,prec);fprintf(stderr,"[C1C-SAMPLER] sampler=%s palette_precision=%s\\n",sampler,prec);
 const char*c1dir=getenv("ENG_C1_ATLAS");'''
 assert old in s;s=s.replace(old,new).replace('c1_index?"index.frag":"race.frag"','c1_index?c1_fragment:"race.frag"');return s
code=source.read_text().replace('raverace/native/generated/c1b/{variant}','raverace/native/generated/c1c/{variant}').replace('spec.loader.exec_module(c1)','spec.loader.exec_module(c1)\nold_c1=c1.apply;c1.apply=lambda n,s,r,a:attribution(n,old_c1(n,s,r,a),r,a)')
sys.argv=[str(source),'/tmp/rr-m3-reference-gl','host','--validation'];exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','attribution':attribution})
