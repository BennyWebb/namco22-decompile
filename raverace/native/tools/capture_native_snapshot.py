#!/usr/bin/env python3
"""Private renderer relink to save ONE immutable snapshot for M2 offline import."""
import argparse,json,pathlib,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--tick',type=int,default=2500);p.add_argument('--capture-world',action='store_true');a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];build=a.build.resolve();out=a.out.resolve()
assert not out.exists() and not out.is_relative_to(root);out.mkdir(parents=True)
s=(root/'raverace/src/rr_gl.c').read_text();needle='static void presentation_geometry(const rr_presentation *snapshot, bool update_palette)\n{';assert s.count(needle)==1
hook=r'''
    if(snapshot->frame==2500){static int saved;if(!saved){saved=1;
        char path[4096];snprintf(path,sizeof path,"%s/direct.bin",M2_CAPTURE_DIR);FILE*f=fopen(path,"wbx");if(!f)abort();
        fwrite(&snapshot->direct_count,4,1,f);fwrite(snapshot->direct,sizeof snapshot->direct[0],snapshot->direct_count,f);fclose(f);
        snprintf(path,sizeof path,"%s/poly.i32",M2_CAPTURE_DIR);f=fopen(path,"wbx");if(!f)abort();fwrite(snapshot->poly,1,sizeof snapshot->poly,f);fclose(f);
        snprintf(path,sizeof path,"%s/pointram.i32",M2_CAPTURE_DIR);f=fopen(path,"wbx");if(!f)abort();fwrite(snapshot->point,1,sizeof snapshot->point,f);fclose(f);
        fprintf(stderr,"[M2-SNAPSHOT] frame=%u direct=%u walk=%d\n",snapshot->frame,snapshot->direct_count,snapshot->walk);
    }}
'''
hook=hook.replace('snapshot->frame==2500',f'snapshot->frame=={a.tick}')
extra='''
        const void*arrays[]={snapshot->pal,snapshot->mixer,snapshot->czram,snapshot->cgram,snapshot->text,snapshot->tilemapattr};
        const size_t sizes[]={sizeof snapshot->pal,sizeof snapshot->mixer,sizeof snapshot->czram,sizeof snapshot->cgram,sizeof snapshot->text,sizeof snapshot->tilemapattr};
        const char*names[]={"palette.planar","mixer.bin","czram.bin","cgram.bin","text.bin","tilemapattr.bin"};
        for(int k=0;k<6;k++){snprintf(path,sizeof path,"%s/%s",M2_CAPTURE_DIR,names[k]);f=fopen(path,"wbx");if(!f)abort();fwrite(arrays[k],1,sizes[k],f);fclose(f);}
'''
hook=hook.replace('fprintf(stderr,"[M2-SNAPSHOT]',extra+'        fprintf(stderr,"[M2-SNAPSHOT]')
s='#define M2_CAPTURE_DIR '+json.dumps(str(out))+'\n'+s.replace(needle,needle+hook);(out/'rr_gl_capture.c').write_text(s)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(n):return shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-DRR_GEOMETRY_INDEX_ORDER','-c',str(out/'rr_gl_capture.c'),'-o',str(out/'rr_gl_capture.o')],check=True)
replacements={}
if a.capture_world:
    inc=(root/'raverace/native/tools/frozen_capture.inc').read_text().replace('g_eng_frame==2500',f'g_eng_frame=={a.tick}').replace('getenv("ENG_ATLAS_CAPTURE")',json.dumps(str(out)))
    (out/'capture.inc').write_text(inc)
    qs=(root/'engine/quad_gles2.c').read_text();at=qs.index('static bool check_gl(');qs=qs[:at]+'#include "capture.inc"\n'+qs[at:]
    marker='const draw_command *c = &commands[i];\n        int sampled=SP_SAMPLE;';assert qs.count(marker)==1
    qs=qs.replace(marker,'const draw_command *c = &commands[i];\n        ac_draw(c);\n        int sampled=SP_SAMPLE;')
    qs=qs.replace('SP_FULL_FRAME_BEGIN();','SP_FULL_FRAME_BEGIN();\n    memcpy(ac_bg,bg,sizeof ac_bg);',1)
    marker='bool ok = flush(stats, END) && (!frame_upload || submit_commands(stats)) && check_gl("end frame");';assert qs.count(marker)==1
    qs=qs.replace(marker,marker+'\n    ac_end();')
    ts=(root/'engine/tex_bake.c').read_text()+\
        '\nint tex_atlas_capture_key(GLuint t,uint32_t key[14]){for(int i=0;i<TEX_CACHE_SIZE;i++){const TexCacheEntry*e=&tex_cache[i];if(!e->occupied||e->gl_texture!=t)continue;key[1]=e->alloc_w;key[2]=e->alloc_h;key[3]=e->min_u;key[4]=e->min_v;key[5]=e->range_u;key[6]=e->range_v;key[7]=e->texbank;key[8]=e->pal_group;key[9]=e->pal_gen;key[10]=e->cmode;key[11]=e->cap;key[12]=e->used_w;key[13]=e->used_h;return 1;}return 0;}\n'
    for name,body,original,extra in [('quad',qs,'/engine/quad_gles2.c.o',['-DRR_ORDERED_SUBMISSION','-DRR_GEOMETRY_INDEX_ORDER']),('tex',ts,'/engine/tex_bake.c.o',['-DRR_TEXTURE_TILE_RUNS'])]:
        (out/(name+'.c')).write_text(body)
        subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),*extra,'-I'+str(out),'-c',str(out/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
        replacements[original]=str(out/(name+'.o'))
cmd=[]
for x in link:
 if x.startswith('-Wl,--dependency-file='):continue
 if x.endswith('/src/rr_gl.c.o'):x=str(out/'rr_gl_capture.o')
 for tail,replacement in replacements.items():
  if x.endswith(tail):x=replacement
 if x=='rr_gles2':x=str(out/'rr_snapshot_capture')
 cmd.append(x)
subprocess.run(cmd,cwd=build,check=True);print(out/'rr_snapshot_capture')
