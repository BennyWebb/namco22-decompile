#version 300 es
precision highp float;
precision highp int;
layout(location=0) in vec4 object_state;
layout(location=1) in vec4 brightness;
uniform highp sampler2D meshes;
uniform highp sampler2D instances;
uniform highp sampler2D dynamic_mesh;
uniform highp sampler2D materials;
flat out highp int palette_group;
flat out highp int source_material;
out highp vec2 texcoord;
out highp vec2 shade_z;
out mediump float priority;
out mediump vec4 fog;
vec4 mesh_read(int index,bool dynamic){return dynamic?texelFetch(dynamic_mesh,ivec2(index,0),0):texelFetch(meshes,ivec2(index%1024,index/1024),0);}
void main(){
 int instance=int(object_state.y), material=int(abs(object_state.w))/128-1;palette_group=int(abs(object_state.w))%128;
 source_material=material;
 vec4 m0=texelFetch(instances,ivec2(0,instance),0),m1=texelFetch(instances,ivec2(1,instance),0),m2=texelFetch(instances,ivec2(2,instance),0),cam=texelFetch(instances,ivec2(3,instance),0);
 vec4 uv=texelFetch(materials,ivec2(material%1024,material/1024),0);
 priority=object_state.w<0.0?1.0:0.0;fog=vec4(0.0);
 bool dynamic=object_state.x<0.0;int base=dynamic?-int(object_state.x)-1:int(object_state.x);
 vec4 source[4];vec2 original_uv[4];vec4 clipped[8];vec2 clipped_uv[8];int count=0;
 for(int i=0;i<4;i++){vec3 local=mesh_read(base+i*2,dynamic).xyz;source[i]=vec4(floor(vec3(dot(local,m0.xyz),dot(local,m1.xyz),dot(local,m2.xyz))/32768.0)+vec3(m0.w,m1.w,m2.w),brightness[i]);original_uv[i]=mesh_read(base+i*2+1,dynamic).xy;}
 for(int i=0;i<4;i++){int j=(i+1)%4;bool a=source[i].z>=1.0,b=source[j].z>=1.0;if(a){clipped[count]=source[i];clipped_uv[count++]=original_uv[i];}if(a!=b){float t=(1.0-source[i].z)/(source[j].z-source[i].z);clipped[count]=mix(source[i],source[j],t);clipped[count].z=1.0;clipped_uv[count++]=mix(original_uv[i],original_uv[j],t);}}
 int tri=gl_VertexID/3,which=gl_VertexID%3,corner=which==0?0:tri+which;
 if(count<3||tri>=count-2){gl_Position=vec4(0.0);texcoord=vec2(0.0);shade_z=vec2(1.0);return;}
 vec4 position=clipped[corner];float z=position.z;
 gl_Position=vec4(cam.x*z+cam.z*position.x,cam.y*z+cam.w*position.y,object_state.z*z,z);
 texcoord=clipped_uv[corner]*uv.xy+uv.zw;shade_z=vec2(position.w*z,z);
}
