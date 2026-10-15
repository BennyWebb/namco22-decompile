#!/usr/bin/env python3
"""Diagnostic upper bound for removing vertex gathers; NOT a native backend.
CPU transforms immutable local meshes every picture only to attribute GPU costs.
"""
import argparse,pathlib,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(exist_ok=False)
root=pathlib.Path(__file__).resolve().parents[2];src=a.out/'control.c'
subprocess.run([sys.executable,str(root/'native/tools/build_profile_race.py'),str(src)],check=True)
s=src.read_text().replace('#include <time.h>','#include <time.h>\n#include <math.h>')
s=s.replace('static double last_end3;', 'static double last_end3;')
mark='static Attribute3 attribute_data3[MAXQ3];'
s=s.replace(mark,mark+'\ntypedef struct {float p[4][4],uv[4][2],cam[4];} Control3;\nstatic Control3 controls3[MAXQ3];static GLuint control_buffer3,index_buffer3;static float *cpu_mesh3;static size_t cpu_mesh_texels3;')
s=s.replace('mesh3=texture3(mesh_width3,mesh_height3,GL_RGBA32F,p);free(p);','mesh3=texture3(mesh_width3,mesh_height3,GL_RGBA32F,p);cpu_mesh3=p;cpu_mesh_texels3=n/16;')
mark='glBindVertexArray(0);\n stream3='
replacement='''glGenBuffers(1,&control_buffer3);glBindBuffer(GL_ARRAY_BUFFER,control_buffer3);glBufferData(GL_ARRAY_BUFFER,sizeof controls3,NULL,GL_STREAM_DRAW);for(unsigned k=2;k<9;k++){glEnableVertexAttribArray(k);glVertexAttribDivisor(k,1);}glGenBuffers(1,&index_buffer3);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,index_buffer3);const uint8_t indices[]={0,1,2,0,2,3};glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof indices,indices,GL_STATIC_DRAW);glBindVertexArray(0);
 stream3='''
assert mark in s;s=s.replace(mark,replacement)
mark=' double prepared=clock3(CLOCK_MONOTONIC);'
replacement=''' for(unsigned i=0;i<h->visible;i++){const Quad3*r=q+visible[i];const float *m=ins[r->instance].data;Control3*c=controls3+i;int base=r->mesh<0?-r->mesh-1:r->mesh;CHECK3(base>=0&&(size_t)(base+7)<(r->mesh<0?h->dynamic:cpu_mesh_texels3));const float*geo=r->mesh<0?dynamic:cpu_mesh3;for(unsigned v=0;v<4;v++){const float*l=geo+(base+v*2)*4;for(unsigned j=0;j<3;j++)c->p[v][j]=floorf((l[0]*m[j*4]+l[1]*m[j*4+1]+l[2]*m[j*4+2])/32768.f)+m[j*4+3];c->p[v][3]=0;c->uv[v][0]=l[4]*material_data3[r->material].uv[0]+material_data3[r->material].uv[2];c->uv[v][1]=l[5]*material_data3[r->material].uv[1]+material_data3[r->material].uv[3];}memcpy(c->cam,m+12,16);}
 double prepared=clock3(CLOCK_MONOTONIC);glBindBuffer(GL_ARRAY_BUFFER,control_buffer3);glBufferSubData(GL_ARRAY_BUFFER,0,h->visible*sizeof(Control3),controls3);'''
assert mark in s;s=s.replace(mark,replacement)
mark='if(profile_mode3!=2)glDrawArraysInstanced(GL_TRIANGLES,0,near?12:6,g->count);'
replacement='''glBindBuffer(GL_ARRAY_BUFFER,control_buffer3);for(unsigned k=2;k<9;k++)glVertexAttribPointer(k,4,GL_FLOAT,GL_FALSE,sizeof(Control3),(void*)(uintptr_t)(g->first*sizeof(Control3)+(k-2)*16));if(profile_mode3!=2){if(near)glDrawArraysInstanced(GL_TRIANGLES,0,12,g->count);else glDrawElementsInstanced(GL_TRIANGLES,6,GL_UNSIGNED_BYTE,0,g->count);}'''
assert mark in s;s=s.replace(mark,replacement).replace('attrs>=2','attrs>=9')
s=s.replace('h->visible*sizeof(Attribute3)+h->instances*64+h->dynamic*16,waited', 'h->visible*(sizeof(Attribute3)+sizeof(Control3))+h->instances*64+h->dynamic*16,waited').replace('sizeof attribute_data3+MAXI3*64+4096*16,font_count3', 'sizeof attribute_data3+sizeof controls3+6+MAXI3*64+4096*16,font_count3')
src.write_text(s)
header='''#version 300 es
precision highp float;
precision highp int;
layout(location=0) in vec4 object_state;
layout(location=1) in vec4 brightness;
layout(location=2) in vec4 p0;
layout(location=3) in vec4 p1;
layout(location=4) in vec4 p2;
layout(location=5) in vec4 p3;
layout(location=6) in vec4 uv01;
layout(location=7) in vec4 uv23;
layout(location=8) in vec4 cam;
out highp vec2 texcoord;
out highp vec2 shade_z;
out mediump float priority;
out mediump vec4 fog;
'''
regular=header+'''void main(){
vec4 points[4]=vec4[4](p0,p1,p2,p3);vec2 uvs[4]=vec2[4](uv01.xy,uv01.zw,uv23.xy,uv23.zw);int corner=gl_VertexID;vec3 position=points[corner].xyz;float z=position.z;
gl_Position=vec4(cam.x*z+cam.z*position.x,cam.y*z+cam.w*position.y,object_state.z*z,z);
texcoord=uvs[corner];shade_z=vec2(brightness[corner]*z,z);priority=object_state.w<0.0?1.0:0.0;fog=vec4(0.0);
}
'''
near=(root/'native/render/race_v3_near.vert').read_text();tail=near[near.index(' for(int i=0;i<4;i++){int j='):]
# UVs have already been mapped; affine interpolation commutes with clipping.
tail=tail.replace('clipped_uv[corner]*uv.xy+uv.zw','clipped_uv[corner]')
near=header+'''void main(){priority=object_state.w<0.0?1.0:0.0;fog=vec4(0.0);
vec4 points[4]=vec4[4](p0,p1,p2,p3);vec4 source[4];vec2 original_uv[4]=vec2[4](uv01.xy,uv01.zw,uv23.xy,uv23.zw);vec4 clipped[8];vec2 clipped_uv[8];int count=0;
for(int i=0;i<4;i++)source[i]=vec4(points[i].xyz,brightness[i]);
'''+tail
(a.out/'race_v3.vert').write_text(regular);(a.out/'race_v3_near.vert').write_text(near);(a.out/'race.frag').write_bytes((root/'native/render/race.frag').read_bytes())
