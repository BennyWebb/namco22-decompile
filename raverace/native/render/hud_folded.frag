#version 100
precision highp float;
varying highp vec2 uv;
uniform sampler2D scene_tex,raw_tex,palette_tex,lut_tex;
uniform bool shadow_on;
void main(){
 vec4 base=texture2D(scene_tex,uv);vec3 c=floor(base.rgb*255.+.5);float row=.125;
 if(base.a<.5){vec4 t=texture2D(raw_tex,uv);if(t.a>.5){float idx=floor(t.r*255.+.5);
  if(shadow_on&&idx>=252.&&idx<=254.)row=(idx-251.+.5)/4.;
  else {gl_FragColor=texture2D(palette_tex,vec2((idx+.5)/256.,.5));return;}
 }}
 vec3 o;o.r=texture2D(lut_tex,vec2((c.r+.5)/256.,row)).r;
 o.g=texture2D(lut_tex,vec2((c.g+.5)/256.,row)).g;
 o.b=texture2D(lut_tex,vec2((c.b+.5)/256.,row)).b;
 gl_FragColor=vec4(o,1.);
}
