static const char *gles3_fold_fs3 =
    "#version 100\nprecision highp float;\nvarying highp vec2 uv;\nuniform sampler2D "
    "scene_tex,raw_tex,palette_tex,lut_tex;\nuniform bool shadow_on;\nvoid main(){\n vec4 "
    "base=texture2D(scene_tex,uv);vec3 c=floor(base.rgb*255.+.5);float row=.125;\n if(base.a<.5){vec4 "
    "t=texture2D(raw_tex,uv);if(t.a>.5){float idx=floor(t.r*255.+.5);\n  "
    "if(shadow_on&&idx>=252.&&idx<=254.)row=(idx-251.+.5)/4.;\n  else "
    "{gl_FragColor=texture2D(palette_tex,vec2((idx+.5)/256.,.5));return;}\n }}\n vec3 "
    "o;o.r=texture2D(lut_tex,vec2((c.r+.5)/256.,row)).r;\n o.g=texture2D(lut_tex,vec2((c.g+.5)/256.,row)).g;\n "
    "o.b=texture2D(lut_tex,vec2((c.b+.5)/256.,row)).b;\n gl_FragColor=vec4(o,1.);\n}\n";
static bool gles3_blank3[1024];
/* Alternate HUD representation: glyph quads produce an 8-bit pen + coverage
 * image. Palette, shadow and world priority remain in the final compositor. */
static GLuint glyph_texture, pen_target, glyph_program, glyph_vbo;
static GLint glyph_sampler;
static bool glyph_valid[1024], mesh_valid;
static uint8_t mesh_tilemap[8192], needed_glyphs[1024];
static int mesh_scroll_x, mesh_scroll_y, glyph_vertex_count;
static float glyph_vertices[41 * 31 * 6 * 5];
static bool gpu_geometry(void)
{
    return true;
}
static const char *glyph_final_fs =
    "#version 100\nprecision highp float;varying highp vec2 uv;"
    "uniform sampler2D scene_tex,raw_tex,palette_tex,lut_tex;uniform bool shadow_on;uniform vec3 mix0,mix1,mix2;"
    "void main(){vec4 base=texture2D(scene_tex,uv);vec3 c=floor(base.rgb*255.+.5);"
    "if(base.a<.5){vec4 t=texture2D(raw_tex,uv);if(t.a>.5){float idx=floor(t.r*255.+.5);"
    "if(shadow_on&&idx>=252.&&idx<=254.){vec3 m=idx<252.5?mix0:(idx<253.5?mix1:mix2);c=floor(c*m/256.);}"
    "else c=floor(texture2D(palette_tex,vec2((idx+.5)/256.,.5)).rgb*255.+.5);}}"
    "vec3 "
    "o;o.r=texture2D(lut_tex,vec2((c.r+.5)/256.,.5)).r;o.g=texture2D(lut_tex,vec2((c.g+.5)/"
    "256.,.5)).g;o.b=texture2D(lut_tex,vec2((c.b+.5)/256.,.5)).b;gl_FragColor=vec4(o,1.);}\n";
static void gpu_account(size_t bytes)
{
    gpu_bytes += bytes;
    gpu_calls++;
    if (rr_video_layers_profile_enabled()) {
        gpu_profile_bytes += bytes;
        gpu_profile_calls++;
    }
}
static bool geometry_open(void)
{
    const char *vs =
        "#version 100\nattribute vec2 position,texcoord;attribute float group;varying highp vec2 uv;varying highp "
        "float pal;void main(){gl_Position=vec4(position.x/320.-1.,1.-position.y/240.,0.,1.);uv=texcoord;pal=group;}";
    const char *fs = "#version 100\nprecision highp float;uniform sampler2D glyph;varying highp vec2 uv;varying highp "
                     "float pal;void main(){float "
                     "p=floor(texture2D(glyph,uv).r*255.+.5);if(p>14.5)discard;gl_FragColor=vec4((floor(pal+.5)*16.+p)/"
                     "255.,0.,0.,1.);}";
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v)
            glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        return false;
    }
    glyph_program = glCreateProgram();
    glAttachShader(glyph_program, v);
    glAttachShader(glyph_program, f);
    glBindAttribLocation(glyph_program, 0, "position");
    glBindAttribLocation(glyph_program, 1, "texcoord");
    glBindAttribLocation(glyph_program, 2, "group");
    glLinkProgram(glyph_program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok;
    glGetProgramiv(glyph_program, GL_LINK_STATUS, &ok);
    if (!ok)
        return false;
    glyph_sampler = glGetUniformLocation(glyph_program, "glyph");
    glGenTextures(1, &glyph_texture);
    glBindTexture(GL_TEXTURE_2D, glyph_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 512, 512, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &pen_target);
    glBindFramebuffer(GL_FRAMEBUFFER, pen_target);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, raw_tex, 0);
    ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenBuffers(1, &glyph_vbo);
    return ok && error("glyph HUD initialization");
}
static bool geometry_prepare(const rr_video_cpu_frame *frame)
{
    double hp_start = hud_stamp3(CLOCK_MONOTONIC), hp_cpu = hud_stamp3(CLOCK_THREAD_CPUTIME_ID);
    bool blank3[1024];
    bool changed3 = false;
    uint8_t transparent3[128];
    memset(transparent3, 255, 128);
    for (unsigned k3 = 0; k3 < 1024; k3++) {
        const uint8_t *p3 = k3 < 960 ? frame->characters + k3 * 128 : frame->tilemap + (k3 - 960) * 128;
        blank3[k3] = !memcmp(p3, transparent3, 128);
        changed3 |= blank3[k3] != gles3_blank3[k3];
    }
    memcpy(gles3_blank3, blank3, sizeof blank3);
    bool rebuild = changed3 || !mesh_valid || mesh_scroll_x != frame->scroll_x || mesh_scroll_y != frame->scroll_y ||
                   memcmp(mesh_tilemap, frame->tilemap, sizeof mesh_tilemap);
    if (rebuild) {
        memset(needed_glyphs, 0, sizeof needed_glyphs);
        glyph_vertex_count = 0;
        for (int y = -(frame->scroll_y & 15); y < H; y += 16)
            for (int x = -(frame->scroll_x & 15); x < W; x += 16) {
                unsigned index = (((y + frame->scroll_y) & 1023) / 16 * 64 + ((x + frame->scroll_x) & 1023) / 16) * 2;
                unsigned word = (frame->tilemap[index] << 8) | frame->tilemap[index + 1], code = word & 1023;
                if (blank3[code])
                    continue;
                needed_glyphs[code] = 1;
                float u0 = (code & 31) * 16 / 512.f, u1 = u0 + 16 / 512.f, v0 = (code >> 5) * 16 / 512.f,
                      v1 = v0 + 16 / 512.f;
                if (word & 1024) {
                    float tmp = u0;
                    u0 = u1;
                    u1 = tmp;
                }
                if (word & 2048) {
                    float tmp = v0;
                    v0 = v1;
                    v1 = tmp;
                }
                const float vertices[] = {x,      y,      u0, v0, word >> 12, x + 16, y,      u1, v0, word >> 12,
                                          x + 16, y + 16, u1, v1, word >> 12, x,      y,      u0, v0, word >> 12,
                                          x + 16, y + 16, u1, v1, word >> 12, x,      y + 16, u0, v1, word >> 12};
                memcpy(glyph_vertices + glyph_vertex_count * 5, vertices, sizeof vertices);
                glyph_vertex_count += 6;
            }
        memcpy(mesh_tilemap, frame->tilemap, sizeof mesh_tilemap);
        mesh_scroll_x = frame->scroll_x;
        mesh_scroll_y = frame->scroll_y;
        mesh_valid = true;
        glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo);
        size_t bytes = glyph_vertex_count * 5 * sizeof(float);
        glBufferData(GL_ARRAY_BUFFER, bytes, glyph_vertices, GL_STREAM_DRAW);
        gpu_account(bytes);
        if (rr_video_layers_profile_enabled())
            gpu_profile_vertex_bytes += bytes;
    }
    bool dirty = rebuild;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glyph_texture);
    for (int code = 0; code < 1024; code++)
        if (needed_glyphs[code]) {
            const uint8_t *source = code < 960 ? frame->characters + code * 128 : frame->tilemap + (code - 960) * 128;
            if (glyph_valid[code] && !memcmp(uploaded_raw + code * 128, source, 128))
                continue;
            uint8_t pens[256];
            for (int i = 0; i < 128; i++) {
                pens[i * 2] = source[i] >> 4;
                pens[i * 2 + 1] = source[i] & 15;
            }
            glTexSubImage2D(
                GL_TEXTURE_2D, 0, (code & 31) * 16, (code >> 5) * 16, 16, 16, GL_LUMINANCE, GL_UNSIGNED_BYTE, pens);
            gpu_account(sizeof pens);
            if (rr_video_layers_profile_enabled())
                gpu_profile_glyph_bytes += sizeof pens;
            memcpy(uploaded_raw + code * 128, source, 128);
            glyph_valid[code] = true;
            dirty = true;
        }
    if (dirty) {
        GLboolean dither = glIsEnabled(GL_DITHER);
        glDisable(GL_DITHER);
        glBindFramebuffer(GL_FRAMEBUFFER, pen_target);
        glViewport(0, 0, W, H);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(glyph_program);
        glUniform1i(glyph_sampler, 0);
        glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(2 * sizeof(float)));
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(4 * sizeof(float)));
        for (int i = 0; i < 3; i++)
            glEnableVertexAttribArray(i);
        glDrawArrays(GL_TRIANGLES, 0, glyph_vertex_count);
        if (dither)
            glEnable(GL_DITHER);
    }
    if (hud_fences3) {
        glFinish();
    }
    hud_pen_wall3 = (hud_stamp3(CLOCK_MONOTONIC) - hp_start) * 1000;
    hud_pen_cpu3 = (hud_stamp3(CLOCK_THREAD_CPUTIME_ID) - hp_cpu) * 1000;
    return error("glyph HUD update");
}
static void geometry_close(void)
{
    if (!glyph_program)
        return;
    glDeleteTextures(1, &glyph_texture);
    glDeleteFramebuffers(1, &pen_target);
    glDeleteBuffers(1, &glyph_vbo);
    glDeleteProgram(glyph_program);
    glyph_program = 0;
}
