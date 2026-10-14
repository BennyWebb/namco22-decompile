/* The GPU owner alone constructs, groups and submits immutable command prefixes.
 * No grouping crosses a texture mutation or an arena-capacity drain. */
static GLuint world_program;
static GLint world_projection,world_sampler,world_priority;
static vertex world_stream[MAX_VERTICES];
static draw_command world_order[MAX_COMMANDS];
static bool world_group_safe;
static bool world_verify;
static GLuint world_reference_fbo,world_reference_texture,world_reference_depth;
static GLint world_main_fbo;
static unsigned world_validated;
static unsigned char world_pixels[2][640*480*4];
static const GLfloat world_matrix[16]={2.f/640,0,0,0,0,-2.f/480,0,0,0,0,1,0,-1,1,0,1};
static bool world_open(void)
{
    if(!world_mode)return true;
    if(!priority_enabled){fprintf(stderr,"[WORLD-ARRAY] legacy blending uses original renderer\n");world_mode=0;return true;}
    if(!frame_upload || diagnostic || white_batches || AUDIT_DIVISOR!=1 || AUDIT_NO_DRAW || AUDIT_FLAT){fprintf(stderr,"[WORLD-ARRAY] requires normal frame-streamed rendering\n");return false;}
    const char *version=(const char*)glGetString(GL_VERSION);
    if(!strstr(version,"OpenGL ES 3.")){fprintf(stderr,"[WORLD-ARRAY] GLES3 unavailable; using original renderer\n");world_mode=0;return true;}
    GLint layers;glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,&layers);if(layers<2)return false;
    const char *vs="#version 300 es\nprecision highp float;in vec3 a_position;in vec4 a_colour;in vec3 a_uvq;in float a_layer;uniform mat4 u_projection;out mediump vec4 v_colour;out highp vec3 v_uvq;out highp float v_layer;void main(){gl_Position=u_projection*vec4(a_position,1.0);v_colour=a_colour;v_uvq=a_uvq;v_layer=a_layer;}";
    const char *fs="#version 300 es\nprecision highp float;uniform lowp sampler2DArray u_texture;uniform bool u_priority;in mediump vec4 v_colour;in highp vec3 v_uvq;in highp float v_layer;out vec4 output_colour;void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec4 texel=textureLod(u_texture,vec3(uv,v_layer),0.0);if(texel.a==0.0)discard;vec4 c=v_colour*texel;output_colour=u_priority?vec4(c.rgb,v_colour.a):c;}";
    GLuint v=compile(GL_VERTEX_SHADER,vs),f=compile(GL_FRAGMENT_SHADER,fs);if(!v||!f)return false;
    world_program=glCreateProgram();glAttachShader(world_program,v);glAttachShader(world_program,f);
    glBindAttribLocation(world_program,0,"a_position");glBindAttribLocation(world_program,1,"a_colour");glBindAttribLocation(world_program,2,"a_uvq");glBindAttribLocation(world_program,3,"a_layer");glLinkProgram(world_program);glDeleteShader(v);glDeleteShader(f);
    GLint ok;glGetProgramiv(world_program,GL_LINK_STATUS,&ok);if(!ok)return false;
    world_projection=glGetUniformLocation(world_program,"u_projection");world_sampler=glGetUniformLocation(world_program,"u_texture");world_priority=glGetUniformLocation(world_program,"u_priority");
    world_verify=getenv("GLES_WORLD_VALIDATE")!=NULL;
    tex_bake_world_arrays(1,world_verify);
    if(world_verify){
        GLint previous;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
        glGenTextures(1,&world_reference_texture);glBindTexture(GL_TEXTURE_2D,world_reference_texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glGenRenderbuffers(1,&world_reference_depth);glBindRenderbuffer(GL_RENDERBUFFER,world_reference_depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,640,480);
        glGenFramebuffers(1,&world_reference_fbo);glBindFramebuffer(GL_FRAMEBUFFER,world_reference_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,world_reference_texture,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,world_reference_depth);
        ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,previous);if(!ok)return false;
    }
    fprintf(stderr,"[WORLD-ARRAY] mode=%s cohort_layers=2 physical_limit=%zu proof=%d streaming_bytes=%zu\n",world_mode==2?"group":"ordered",2*tex_cache_budget,world_verify,sizeof world_stream);
    tex_bake_gles2_binding_reset();return check_gl("world array init");
}
static void world_use(int array)
{
    glUseProgram(array?world_program:program);
    glUniformMatrix4fv(array?world_projection:projection,1,GL_FALSE,world_matrix);
    glUniform1i(array?world_sampler:sampler,0);
    GLint p=array?world_priority:priority_uniform;if(p>=0)glUniform1i(p,priority_enabled);
    if(array){glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(vertex),(void*)offsetof(vertex,layer));glEnableVertexAttribArray(3);}else glDisableVertexAttribArray(3);
}
static void world_begin(const float bg[3],int count)
{
    if(!world_mode)return;
    GLint bits;GLfloat range[2];glGetIntegerv(GL_DEPTH_BITS,&bits);glGetFloatv(GL_DEPTH_RANGE,range);
    world_group_safe=world_mode==2 && priority_enabled && bits>=16 && range[0]==0 && range[1]==1 && .9*65535/(count+1)>2;
    if(world_verify){
        const char *tick=getenv("GLES_WORLD_CAPTURE_TICK");
        if(tick && multi_present_tick==(unsigned)strtoul(tick,NULL,10)){char path[1024];snprintf(path,sizeof path,"%s/scene.meta",getenv("GLES_WORLD_VALIDATE"));FILE *f=fopen(path,"wx");if(f){fprintf(f,"%.9g %.9g %.9g %d %d\n",bg[0],bg[1],bg[2],priority_enabled,bits);fclose(f);}}
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&world_main_fbo);glBindFramebuffer(GL_FRAMEBUFFER,world_reference_fbo);
        glClearColor(bg[0],bg[1],bg[2],priority_enabled?0:1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glBindFramebuffer(GL_FRAMEBUFFER,world_main_fbo);
    }
}
static int world_compare_commands(const void *a,const void *b)
{
    const draw_command *x=a,*y=b;
    if(x->array!=y->array)return x->array<y->array?-1:1;
    if(x->texture!=y->texture)return x->texture<y->texture?-1:1;
    for(int i=0;i<4;i++)if(x->scissor[i]!=y->scissor[i])return x->scissor[i]<y->scissor[i]?-1:1;
    /* Distinct ranks are guaranteed at least two conservative 16-bit depth
     * units apart. Stable original order handles an identical rank/tie. */
    float zx=batch[x->first].position[2],zy=batch[y->first].position[2];
    if(zx!=zy)return zx<zy?-1:1;
    return (x->first>y->first)-(x->first<y->first);
}
static bool world_submit(eng_gles2_stats *stats)
{
    if(!command_count)return true;
    double start=seconds();eng_audit_scope audit_submit=eng_audit_begin(AUD_SUBMIT);
    const char *capture_tick=getenv("GLES_WORLD_CAPTURE_TICK");
    if(world_verify && capture_tick && multi_present_tick==(unsigned)strtoul(capture_tick,NULL,10)){
        const char *dir=getenv("GLES_WORLD_VALIDATE");char path[1024];
        snprintf(path,sizeof path,"%s/draws.bin",dir);FILE *draws=fopen(path,"ab");if(!draws)return false;
        GLint old_fbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&old_fbo);GLuint capture_fbo;glGenFramebuffers(1,&capture_fbo);
        for(int i=0;i<command_count;i++){
            const draw_command *c=commands+i;
            snprintf(path,sizeof path,"%s/command-resources.txt",dir);FILE *metadata=fopen(path,"a");if(metadata){fprintf(metadata,"%d %u %u %d %d %d\n",i,c->reference,c->texture,c->layer,c->array,c->count);fclose(metadata);}
            uint32_t header[6]={c->reference,(unsigned)c->count,c->scissor[0],c->scissor[1],c->scissor[2],c->scissor[3]};
            fwrite(header,4,6,draws);for(int j=0;j<c->count;j++)fwrite(batch+c->first+j,1,40,draws);
            int w,h;tex_bake_world_dimensions(c->reference,&w,&h);
            unsigned char *pixels=malloc((size_t)w*h*8);if(!pixels)return false;
            glBindFramebuffer(GL_FRAMEBUFFER,capture_fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c->reference,0);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
            snprintf(path,sizeof path,"%s/texture-%u.rgba",dir,c->reference);FILE *texture=fopen(path,"wb");if(!texture)return false;fprintf(texture,"%d %d\n",w,h);fwrite(pixels,4,(size_t)w*h,texture);fclose(texture);
            if(c->array){glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,c->texture,0,c->layer);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels+(size_t)w*h*4);
                if(memcmp(pixels,pixels+(size_t)w*h*4,(size_t)w*h*4))fprintf(stderr,"[WORLD-RESOURCE-DIFF] command=%d ref=%u array=%u layer=%d dims=%dx%d\n",i,c->reference,c->texture,c->layer,w,h);
            }
            free(pixels);
        }
        fclose(draws);glBindFramebuffer(GL_FRAMEBUFFER,old_fbo);glDeleteFramebuffers(1,&capture_fbo);tex_bake_gles2_binding_reset();
    }
    memcpy(world_order,commands,(size_t)command_count*sizeof *commands);
    for(int i=0;i<command_count;i++){
        draw_command *c=world_order+i;c->sensitive=0;
        for(int j=0;j<c->count;j+=3){
            const float *a=batch[c->first+j].position,*b=batch[c->first+j+1].position,*v=batch[c->first+j+2].position;
            double area=((double)b[0]-a[0])*((double)v[1]-a[1])-((double)v[0]-a[0])*((double)b[1]-a[1]);
            if(area==0){c->sensitive=1;break;}
        }
    }
    if(world_group_safe)qsort(world_order,command_count,sizeof *world_order,world_compare_commands);
    int cursor=0;
    for(int i=0;i<command_count;i++){
        draw_command *c=world_order+i;memcpy(world_stream+cursor,batch+c->first,(size_t)c->count*sizeof(vertex));c->first=cursor;cursor+=c->count;
    }
    double upload=seconds();glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,(size_t)cursor*sizeof(vertex),world_stream,GL_STREAM_DRAW);
    stats->vertex_upload_seconds+=seconds()-upload;stats->vertex_uploads++;stats->vertex_upload_bytes+=(size_t)cursor*sizeof(vertex);
    glEnable(GL_SCISSOR_TEST);glActiveTexture(GL_TEXTURE0);
    int shader=-1;
    for(int i=0;i<command_count;){
        const draw_command *c=world_order+i;int end=i+1,n=c->count;
        while(!c->sensitive && end<command_count && !world_order[end].sensitive && world_order[end].array==c->array && world_order[end].texture==c->texture && !memcmp(world_order[end].scissor,c->scissor,sizeof c->scissor)){n+=world_order[end++].count;}
        if(shader!=c->array){world_use(c->array);shader=c->array;}
        glScissor(c->scissor[0],c->scissor[1],c->scissor[2],c->scissor[3]);
        tex_bake_gles2_bind(c->array?GL_TEXTURE_2D_ARRAY:GL_TEXTURE_2D,c->texture);
        double call=seconds();glDrawArrays(GL_TRIANGLES,c->first,n);stats->draw_call_seconds+=seconds()-call;stats->triangles+=(unsigned)n/3;stats->draws++;i=end;
    }
    if(world_verify){
        glBindFramebuffer(GL_FRAMEBUFFER,world_reference_fbo);world_use(0);
        /* Reference consumes the unchanged original command prefix and proof
         * textures created from the same decoder bytes at each normal miss. */
        glBufferData(GL_ARRAY_BUFFER,(size_t)batch_first*sizeof(vertex),batch,GL_STREAM_DRAW);
        for(int i=0;i<command_count;i++){
            const draw_command *c=commands+i;glScissor(c->scissor[0],c->scissor[1],c->scissor[2],c->scissor[3]);glBindTexture(GL_TEXTURE_2D,c->reference);glDrawArrays(GL_TRIANGLES,c->first,c->count);
        }
        glBindFramebuffer(GL_FRAMEBUFFER,world_main_fbo);
    }
    world_use(0);tex_bake_gles2_binding_reset();
    bool ok=check_gl("world array prefix");
    if(used)memmove(batch,batch+batch_first,(size_t)used*sizeof(vertex));
    batch_first=command_count=0;
    stats->draw_seconds+=seconds()-start;eng_audit_end(audit_submit);return ok;
}
static bool world_end(void)
{
    if(!world_mode || !world_verify)return true;
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,world_pixels[1]);glBindFramebuffer(GL_FRAMEBUFFER,world_reference_fbo);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,world_pixels[0]);glBindFramebuffer(GL_FRAMEBUFFER,world_main_fbo);
    unsigned changed=0;for(unsigned i=0;i<640*480;i++)if(memcmp(world_pixels[0]+4*i,world_pixels[1]+4*i,4)){
        if(changed<8)fprintf(stderr,"[WORLD-PIXEL] tick=%u x=%u y=%u baseline=%u,%u,%u,%u atlas=%u,%u,%u,%u\n",multi_present_tick,i%640,479-i/640,world_pixels[0][4*i],world_pixels[0][4*i+1],world_pixels[0][4*i+2],world_pixels[0][4*i+3],world_pixels[1][4*i],world_pixels[1][4*i+1],world_pixels[1][4*i+2],world_pixels[1][4*i+3]);
        changed++;
    }
    if(changed){const char *dir=getenv("GLES_WORLD_VALIDATE");char path[1024];for(int i=0;i<2;i++){snprintf(path,sizeof path,"%s/%u-%s.rgba",dir,multi_present_tick,i?"atlas":"reference");FILE *f=fopen(path,"wx");if(f){fwrite(world_pixels[i],1,sizeof world_pixels[i],f);fclose(f);}}fprintf(stderr,"[WORLD-VALIDATE-FAIL] tick=%u pixels=%u\n",multi_present_tick,changed);return false;}
    world_validated++;if(world_validated%120==0)fprintf(stderr,"[WORLD-VALIDATE-PASS] tick=%u frames=%u pixels_differing=0\n",multi_present_tick,world_validated);
    return check_gl("world exact reference");
}
static void world_close(void)
{
    if(world_program)glDeleteProgram(world_program);
    if(world_reference_fbo)glDeleteFramebuffers(1,&world_reference_fbo);
    if(world_reference_texture)glDeleteTextures(1,&world_reference_texture);
    if(world_reference_depth)glDeleteRenderbuffers(1,&world_reference_depth);
    if(world_verify)fprintf(stderr,"[WORLD-VALIDATE-TOTAL] frames=%u\n",world_validated);
}
