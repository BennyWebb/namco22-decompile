#version 300 es
precision highp float;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
out vec4 colour;
// Diagnostic only: preserves active UV/brightness varying work, removes image read/discard.
void main(){colour=vec4(fract(texcoord)*0.25+0.25,clamp(shade_z.x/shade_z.y,0.0,1.0),priority);}
