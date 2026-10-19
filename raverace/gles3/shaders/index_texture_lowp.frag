#version 300 es
precision highp float;
precision highp int;
uniform highp usampler2D image;
uniform lowp sampler2D palette;
flat in highp int palette_group;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
out vec4 colour;
void main()
{
    ivec2 size = textureSize(image, 0);
    ivec2 xy = clamp(ivec2(floor(texcoord * vec2(size))), ivec2(0), size - 1);
    uint pen = texture(image, texcoord).r;
    vec4 t = texelFetch(palette, ivec2(int(pen), palette_group), 0);

    colour = vec4(t.rgb * (shade_z.x / shade_z.y), priority);
}
