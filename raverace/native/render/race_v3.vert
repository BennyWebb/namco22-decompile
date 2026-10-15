#version 300 es
precision highp float;
precision highp int;
layout(location=0) in vec4 object_state;
layout(location=1) in vec4 brightness;
uniform highp sampler2D meshes;
uniform highp sampler2D instances;
uniform highp sampler2D dynamic_mesh;
uniform highp sampler2D materials;
out highp vec2 texcoord;
out highp vec2 shade_z;
out mediump float priority;
out mediump vec4 fog;
vec4 mesh_read(int index,bool dynamic){return dynamic?texelFetch(dynamic_mesh,ivec2(index,0),0):texelFetch(meshes,ivec2(index%1024,index/1024),0);}
void main(){
 int instance=int(object_state.y), material=int(abs(object_state.w))-1;
 vec4 m0=texelFetch(instances,ivec2(0,instance),0),m1=texelFetch(instances,ivec2(1,instance),0),m2=texelFetch(instances,ivec2(2,instance),0),cam=texelFetch(instances,ivec2(3,instance),0);
 vec4 uv=texelFetch(materials,ivec2(material%1024,material/1024),0);
 priority=object_state.w<0.0?1.0:0.0;fog=vec4(0.0);
 bool dynamic=object_state.x<0.0;int base=dynamic?-int(object_state.x)-1:int(object_state.x);
 int id=gl_VertexID;int corner=id==0||id==3?0:id==1?1:id==2||id==4?2:3;
 vec3 local=mesh_read(base+corner*2,dynamic).xyz;
 vec3 position=floor(vec3(dot(local,m0.xyz),dot(local,m1.xyz),dot(local,m2.xyz))/32768.0)+vec3(m0.w,m1.w,m2.w);
 float z=position.z;gl_Position=vec4(cam.x*z+cam.z*position.x,cam.y*z+cam.w*position.y,object_state.z*z,z);
 texcoord=mesh_read(base+corner*2+1,dynamic).xy*uv.xy+uv.zw;shade_z=vec2(brightness[corner]*z,z);
}
