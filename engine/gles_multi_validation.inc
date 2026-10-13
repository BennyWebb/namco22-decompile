/* Optional exact reference draws over normally selected, live texture resources.
 * Each chunk is replayed before its textures may be mutated/deleted. No cache
 * lookups, tier overrides or draw reordering occur in the reference path. */
static unsigned multi_present_tick;
void eng_gles2_present_tick(unsigned tick) { multi_present_tick=tick; }
#ifdef RR_GLES2_MULTITEXTURE_BATCH
static GLuint compile(GLenum type,const char *source);
static const char *mv_vs,*mv_fs,*mv_dir;
static GLuint mv_program,mv_target,mv_texture,mv_depth;
static GLint mv_projection,mv_sampler,mv_priority,mv_main_target;
static unsigned mv_from=1,mv_to=~0u,mv_stride=1,mv_frames,mv_chunks;
static bool mv_active;
static unsigned char *mv_baseline,*mv_candidate;
static bool mv_open(void)
{
    mv_dir=getenv("GLES_MULTI_VALIDATE");if(!mv_dir)return true;
    const char *e=getenv("GLES_MULTI_VALIDATE_FROM");if(e)mv_from=strtoul(e,NULL,10);
    e=getenv("GLES_MULTI_VALIDATE_TO");if(e)mv_to=strtoul(e,NULL,10);
    e=getenv("GLES_MULTI_VALIDATE_STRIDE");if(e)mv_stride=strtoul(e,NULL,10);
    if(!mv_stride || multi_mode!=MULTI_BATCH || !frame_upload || AUDIT_DIVISOR!=1)return false;
    GLuint v=compile(GL_VERTEX_SHADER,mv_vs),f=compile(GL_FRAGMENT_SHADER,mv_fs);
    if(!v||!f)return false;
    mv_program=glCreateProgram();glAttachShader(mv_program,v);glAttachShader(mv_program,f);
    glBindAttribLocation(mv_program,0,"a_position");glBindAttribLocation(mv_program,1,"a_colour");glBindAttribLocation(mv_program,2,"a_uvq");glLinkProgram(mv_program);
    glDeleteShader(v);glDeleteShader(f);GLint ok;glGetProgramiv(mv_program,GL_LINK_STATUS,&ok);if(!ok)return false;
    mv_projection=glGetUniformLocation(mv_program,"u_projection");mv_sampler=glGetUniformLocation(mv_program,"u_texture");mv_priority=glGetUniformLocation(mv_program,"u_priority");
    GLint previous;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
    glGenTextures(1,&mv_texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,mv_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,640,480,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenRenderbuffers(1,&mv_depth);glBindRenderbuffer(GL_RENDERBUFFER,mv_depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,640,480);
    glGenFramebuffers(1,&mv_target);glBindFramebuffer(GL_FRAMEBUFFER,mv_target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,mv_texture,0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,mv_depth);
    ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,previous);tex_bake_gles2_binding_reset();
    mv_baseline=malloc(640*480*4);mv_candidate=malloc(640*480*4);
    fprintf(stderr,"[MULTI-VALIDATE] reference=live_normal_resources range=%u..%u stride=%u additional_CPU_bytes=%u additional_GPU_bytes=%u\n",mv_from,mv_to,mv_stride,640*480*8,640*480*6);
    return ok && mv_baseline && mv_candidate && glGetError()==GL_NO_ERROR;
}
static bool mv_begin(const float bg[3])
{
    mv_active=mv_dir && multi_present_tick>=mv_from && multi_present_tick<=mv_to && (multi_present_tick-mv_from)%mv_stride==0;
    if(!mv_active)return true;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&mv_main_target);glBindFramebuffer(GL_FRAMEBUFFER,mv_target);
    glClearColor(bg[0],bg[1],bg[2],priority_enabled?0:1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(mv_program);
    const GLfloat matrix[16]={2.f/640,0,0,0,0,-2.f/480,0,0,0,0,1,0,-1,1,0,1};
    glUniformMatrix4fv(mv_projection,1,GL_FALSE,matrix);glUniform1i(mv_sampler,0);if(mv_priority>=0)glUniform1i(mv_priority,priority_enabled);
    glUseProgram(program);glBindFramebuffer(GL_FRAMEBUFFER,mv_main_target);return glGetError()==GL_NO_ERROR;
}
static bool mv_chunk(void)
{
    if(!mv_active)return true;
    glBindFramebuffer(GL_FRAMEBUFFER,mv_target);glUseProgram(mv_program);glDisableVertexAttribArray(3);glActiveTexture(GL_TEXTURE0);
    for(int i=0;i<command_count;i++){
        const draw_command *c=&commands[i];glScissor(c->scissor[0],c->scissor[1],c->scissor[2],c->scissor[3]);
        glBindTexture(GL_TEXTURE_2D,white_batches?white:c->texture);glDrawArrays(GL_TRIANGLES,c->first,c->count);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,mv_main_target);glUseProgram(program);glEnableVertexAttribArray(3);tex_bake_gles2_binding_reset();mv_chunks++;
    return glGetError()==GL_NO_ERROR;
}
static bool mv_end(void)
{
    if(!mv_active)return true;
    glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,mv_candidate);glBindFramebuffer(GL_FRAMEBUFFER,mv_target);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,mv_baseline);glBindFramebuffer(GL_FRAMEBUFFER,mv_main_target);
    unsigned changed=0;for(unsigned i=0;i<640*480;i++)if(memcmp(mv_baseline+4*i,mv_candidate+4*i,4)){
        if(changed<8)fprintf(stderr,"[MULTI-PIXEL] frame=%u x=%u y=%u baseline=%u,%u,%u,%u candidate=%u,%u,%u,%u\n",multi_present_tick,i%640,479-i/640,mv_baseline[4*i],mv_baseline[4*i+1],mv_baseline[4*i+2],mv_baseline[4*i+3],mv_candidate[4*i],mv_candidate[4*i+1],mv_candidate[4*i+2],mv_candidate[4*i+3]);
        changed++;
    }
    mv_frames++;if(changed){
        char path[1024];for(int v=0;v<2;v++){snprintf(path,sizeof path,"%s/%u-%s.rgba",mv_dir,multi_present_tick,v?"batch":"reference");FILE *f=fopen(path,"wx");if(f){fwrite(v?mv_candidate:mv_baseline,1,640*480*4,f);fclose(f);}}
        fprintf(stderr,"[MULTI-VALIDATE-FAIL] frame=%u pixels=%u\n",multi_present_tick,changed);return false;
    }
    if(mv_frames%120==0 || multi_present_tick==mv_to)fprintf(stderr,"[MULTI-VALIDATE-PASS] frame=%u validated=%u chunks=%u pixels_differing=0\n",multi_present_tick,mv_frames,mv_chunks);
    return glGetError()==GL_NO_ERROR;
}
static void mv_close(void)
{
    if(!mv_dir)return;
    fprintf(stderr,"[MULTI-VALIDATE-TOTAL] frames=%u chunks=%u\n",mv_frames,mv_chunks);
    glDeleteProgram(mv_program);glDeleteFramebuffers(1,&mv_target);glDeleteRenderbuffers(1,&mv_depth);glDeleteTextures(1,&mv_texture);free(mv_baseline);free(mv_candidate);
}
#else
#define mv_chunk() true
#define mv_begin(bg) true
#define mv_end() true
#endif
