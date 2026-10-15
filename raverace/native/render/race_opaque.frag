#version 300 es
precision highp float;
uniform sampler2D image;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
in mediump vec4 fog;
out vec4 colour;
// Production GLES2 explicitly bypasses polygon fog; preserve that appearance.
void main(){vec4 t=texture(image,texcoord);colour=vec4(t.rgb*(shade_z.x/shade_z.y),priority);}
