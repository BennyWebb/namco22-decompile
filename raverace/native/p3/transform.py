"""P3-only derivation on selected P2 b. Never edits shared engine sources."""
import pathlib,re

def geometry(source):
 start=source.index('static inline int32_t proj_sat(');end=source.index('static inline int32_t sat32(',start)
 original=source[start:end].replace('static inline int32_t proj_sat(', 'static inline int32_t p3_original_projection(')
 return source[:start]+original+'static _Thread_local bool p3_native_projection;\n#include "'+str(pathlib.Path(__file__).resolve().parent/'projection.h')+'"\nstatic inline int32_t proj_sat(int64_t b,int64_t v,int32_t m,int s,int64_t z,int n){return p3_native_projection?p3_projection(b,v,m,s,z,n):p3_original_projection(b,v,m,s,z,n);}\n\n'+source[end:]

def culling(s,verify):
 start=s.index('static unsigned live_cull(');end=s.index('static void live_compile(',start);original=s[start:end]
 replacement=original.replace('static unsigned live_cull(', 'static unsigned p3_cull(')
 old='for(int a=0;a<2;a++)for(int b=0;b<2;b++){double z=b?high[v][2]:low[v][2],x=(a?high[v][0]:low[v][0])/z*zoom,y=(a?high[v][1]:low[v][1])/z*zoom;xl=fmin(xl,x);xh=fmax(xh,x);yl=fmin(yl,y);yh=fmax(yh,y);}'
 new='''if(isfinite(zoom)&&zoom>=0){double zl=low[v][2],zh=high[v][2];
 xl=fmin(xl,low[v][0]/(low[v][0]<0?zl:zh)*zoom);xh=fmax(xh,high[v][0]/(high[v][0]<0?zh:zl)*zoom);
 yl=fmin(yl,low[v][1]/(low[v][1]<0?zl:zh)*zoom);yh=fmax(yh,high[v][1]/(high[v][1]<0?zh:zl)*zoom);
 }else{'''+old+'''}'''
 assert old in replacement;replacement=replacement.replace(old,new)
 if verify:
  replacement=original.replace('static unsigned live_cull(', 'static unsigned p3_cull_original(')+replacement+'static unsigned live_cull(const M3Quad*r){unsigned a=p3_cull_original(r),b=p3_cull(r);if(a!=b){fprintf(stderr,"P3 cull mismatch %u %u\\n",a,b);abort();}return b;}\n'
 else:replacement+='static unsigned live_cull(const M3Quad*r){return p3_cull(r);}\n'
 return s[:start]+replacement+s[end:]

def apply(name,s,root,a):
 if name=='presentation.c':
  s='#include "'+str(root/'raverace/native/p3/priority.h')+'"\n'+s
  # Header needs original includes/types, so include it after presentation's own headers.
  s=s.replace('#include "'+str(root/'raverace/native/p3/priority.h')+'"\n','')
  index=s.index('static ');s=s[:index]+'#include "'+str(root/'raverace/native/p3/priority.h')+'"\n'+s[index:]
  s=s.replace('    double start = now(), cpu = stamp(CLOCK_THREAD_CPUTIME_ID);','    p3_priority(snapshot->frame,2);\n    double start = now(), cpu = stamp(CLOCK_THREAD_CPUTIME_ID);')
  s=s.replace('        double wall=now(),cpu=stamp(CLOCK_THREAD_CPUTIME_ID);','        p3_priority(s->frame,1);\n        double wall=now(),cpu=stamp(CLOCK_THREAD_CPUTIME_ID);')
  s=s.replace('bool rr_presentation_video(uint32_t frame, bool slave)\n{','bool rr_presentation_video(uint32_t frame, bool slave)\n{\n    p3_priority(frame,0);')
 if name=='live.c' and a.step in ['b1','b2']:
  match=re.search(r'#include "([^"\n]+/geometry.c)"',s);assert match
  s=s[:match.start()]+geometry(pathlib.Path(match[1]).read_text())+s[match.end():]
  s=s.replace('bool rr_p1_prepare(rr_presentation *s){', 'bool rr_p1_prepare(rr_presentation *s){p3_native_projection=false;')
  s=s.replace('if(setjmp(p1_unsupported)){p1_fallbacks++;', 'if(setjmp(p1_unsupported)){p3_native_projection=false;p1_fallbacks++;')
  s=s.replace(' if(s->walk){eng_list_cfg cfg=', ' p3_native_projection=true;\n if(s->walk){eng_list_cfg cfg=')
  s=s.replace(' s->native_walk_ms=(clock3', ' p3_native_projection=false;\n s->native_walk_ms=(clock3')
  if a.step=='b2':s=culling(s,a.validation)
 return s
