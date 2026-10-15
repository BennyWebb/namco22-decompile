#version 300 es
precision highp float;
precision highp int;
uniform highp sampler2D meshes;
uniform highp sampler2D commands;
uniform highp sampler2D dynamic_mesh;
uniform int first_command;
out highp vec2 texcoord;
out highp vec2 shade_z;
out mediump float priority;
out mediump vec4 fog;
vec4 mesh_read(int index, bool dyn){return dyn?texelFetch(dynamic_mesh,ivec2(index,0),0):texelFetch(meshes,ivec2(index%1024,index/1024),0);}
void main(){
 int row=first_command+gl_InstanceID;
 vec4 m0=texelFetch(commands,ivec2(0,row),0),m1=texelFetch(commands,ivec2(1,row),0),m2=texelFetch(commands,ivec2(2,row),0);
 vec4 cam=texelFetch(commands,ivec2(3,row),0),uv=texelFetch(commands,ivec2(4,row),0),bri=texelFetch(commands,ivec2(5,row),0),state=texelFetch(commands,ivec2(6,row),0);
 fog=texelFetch(commands,ivec2(7,row),0);priority=state.z;
 bool dyn=state.x<0.0;int base=dyn?-int(state.x)-1:int(state.x);
 int id=gl_VertexID;int corner=id==0||id==3?0:id==1?1:id==2||id==4?2:3;
 vec4 p;vec2 st;
 if(state.w==0.0){
  if(id>=6){gl_Position=vec4(0.0);texcoord=vec2(0);shade_z=vec2(1);return;}
  vec3 local=mesh_read(base+corner*2,dyn).xyz;
  p=vec4(floor(vec3(dot(local,m0.xyz),dot(local,m1.xyz),dot(local,m2.xyz))/32768.0)+vec3(m0.w,m1.w,m2.w),bri[corner]);st=mesh_read(base+corner*2+1,dyn).xy;
 }else{
  vec4 source[4];vec2 original_uv[4];vec4 clipped[8];vec2 clipped_uv[8];int count=0;
  for(int i=0;i<4;i++){vec3 local=mesh_read(base+i*2,dyn).xyz;source[i]=vec4(floor(vec3(dot(local,m0.xyz),dot(local,m1.xyz),dot(local,m2.xyz))/32768.0)+vec3(m0.w,m1.w,m2.w),bri[i]);original_uv[i]=mesh_read(base+i*2+1,dyn).xy;}
  for(int i=0;i<4;i++){int j=(i+1)%4;bool a=source[i].z>=1.0,b=source[j].z>=1.0;if(a){clipped[count]=source[i];clipped_uv[count++]=original_uv[i];}if(a!=b){float t=(1.0-source[i].z)/(source[j].z-source[i].z);clipped[count]=mix(source[i],source[j],t);clipped[count].z=1.0;clipped_uv[count++]=mix(original_uv[i],original_uv[j],t);}}
  int tri=id/3,which=id%3;corner=which==0?0:tri+which;
  if(count<3||tri>=count-2){gl_Position=vec4(0.0);texcoord=vec2(0);shade_z=vec2(1);return;}
  p=clipped[corner];st=clipped_uv[corner];
 }
 float z=p.z;gl_Position=vec4(cam.x*z+cam.z*p.x,cam.y*z+cam.w*p.y,state.y*z,z);texcoord=st*uv.xy+uv.zw;shade_z=vec2(p.w*z,z);
}
