/* Targeted standalone scene capture; never compiled into the playable build. */
static FILE *ac_file;static GLuint ac_fbo;static float ac_bg[3];
static uint32_t ac_keys[4096][14];static unsigned ac_n;
extern int tex_atlas_capture_key(GLuint,uint32_t[14]);
static int ac_active(void){return g_eng_frame==2500&&getenv("ENG_ATLAS_CAPTURE");}
static void ac_draw(const draw_command *c){
 if(!ac_active())return;
 const char *dir=getenv("ENG_ATLAS_CAPTURE");char path[1024];
 if(!ac_file){snprintf(path,sizeof path,"%s/draws.bin",dir);ac_file=fopen(path,"wx");if(!ac_file)exit(2);glGenFramebuffers(1,&ac_fbo);snprintf(path,sizeof path,"%s/scene.meta",dir);FILE *f=fopen(path,"wx");GLint depth;glGetIntegerv(GL_DEPTH_BITS,&depth);fprintf(f,"%.9g %.9g %.9g %d %d\n",ac_bg[0],ac_bg[1],ac_bg[2],priority_enabled,depth);fclose(f);}
 uint32_t key[14]={0};key[0]=c->texture;
 if(c->texture==white){key[1]=key[2]=1;}else if(!tex_atlas_capture_key(c->texture,key))exit(2);
 unsigned id=0;for(;id<ac_n;id++)if(!memcmp(key,ac_keys[id],sizeof key))break;
 if(id==ac_n){if(ac_n==4096)exit(2);memcpy(ac_keys[ac_n++],key,sizeof key);int w=key[1],h=key[2];uint8_t *p=malloc((size_t)w*h*4);if(!p)exit(2);GLint old;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&old);glBindFramebuffer(GL_FRAMEBUFFER,ac_fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c->texture,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)exit(2);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,p);glBindFramebuffer(GL_FRAMEBUFFER,old);
 snprintf(path,sizeof path,"%s/texture-%u.rgba",dir,id);FILE *f=fopen(path,"wx");fprintf(f,"%d %d\n",w,h);fwrite(p,4,(size_t)w*h,f);fclose(f);free(p);
 snprintf(path,sizeof path,"%s/resources.txt",dir);f=fopen(path,"a");fprintf(f,"%u",id);for(int j=0;j<14;j++)fprintf(f," %u",key[j]);fputc('\n',f);fclose(f);
 }
 uint32_t header[6]={id,c->count,c->scissor[0],c->scissor[1],c->scissor[2],c->scissor[3]};fwrite(header,4,6,ac_file);fwrite(batch+c->first,sizeof(vertex),c->count,ac_file);
}
static void ac_end(void){if(!ac_active()||!ac_file)return;fflush(ac_file);char path[1024];snprintf(path,sizeof path,"%s/reference.rgba",getenv("ENG_ATLAS_CAPTURE"));FILE *f=fopen(path,"wx");uint8_t *p=malloc(640*480*4);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,p);fwrite(p,4,640*480,f);fclose(f);free(p);fprintf(stderr,"[ATLAS-CAPTURE] resources=%u\n",ac_n);}
