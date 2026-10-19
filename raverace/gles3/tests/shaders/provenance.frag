#version 300 es
precision highp float;
precision highp int;
uniform highp usampler2D image;
uniform lowp sampler2D palette;
flat in highp int palette_group;
uniform highp usampler2D coordinate_image;
uniform highp usampler2D coordinate_y;
uniform highp isampler2D source_mapping;
flat in highp int source_material;
in highp vec2 texcoord;
in highp vec2 shade_z;
in mediump float priority;
layout(location = 0) out highp uvec4 provenance;
layout(location = 1) out vec4 colour_proof;
layout(location = 2) out vec4 sampled_texel;
void main()
{
    uvec4 bytes =
        uvec4(texture(coordinate_image, vec2(texcoord.x, 0.5)).rg, texture(coordinate_y, vec2(0.5, texcoord.y)).rg);
    ivec2 xy = ivec2(bytes.x | (bytes.y << 8), bytes.z | (bytes.w << 8));
    int at = source_material * 2;
    ivec4 a = texelFetch(source_mapping, ivec2(at % 1024, at / 1024), 0),
          b = texelFetch(source_mapping, ivec2((at + 1) % 1024, (at + 1) / 1024), 0);
    provenance = uvec4(a.x, (a.y + xy.x - a.w) & 4095, (a.z + xy.y - b.x) & 4095, source_material + 1);
    uint pen = texture(image, texcoord).r;
    vec4 t = texelFetch(palette, ivec2(int(pen), palette_group), 0);
    colour_proof = vec4(t.rgb * (shade_z.x / shade_z.y), priority);
    sampled_texel = t;
}
