static bool c1d_debug;
static GLuint c1d_fbo,c1d_target,c1d_proof,c1d_texel,c1d_depth,c1d_coordinates[16],c1d_mapping;
static uint32_t c1d_pixels[640*480*4];
static uint8_t c1d_proof_pixels[640*480*4],c1d_texel_pixels[640*480*4];
extern void rr_c1d_frame(unsigned,const void*,const void*,const void*);
static void c1d_init(const char*dir){
 if(!c1d_debug)return;
 for(unsigned p=0;p<page_count3;p++){
  unsigned w=widths3[p],h=heights3[p];uint8_t*grid=malloc((size_t)w*h*4);CHECK3(grid);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){size_t k=((size_t)y*w+x)*4;grid[k]=x;grid[k+1]=x>>8;grid[k+2]=y;grid[k+3]=y>>8;}
  glGenTextures(1,c1d_coordinates+p);glBindTexture(GL_TEXTURE_2D,c1d_coordinates[p]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,c1_index?GL_RGBA8UI:GL_RGBA8,w,h,0,c1_index?GL_RGBA_INTEGER:GL_RGBA,GL_UNSIGNED_BYTE,grid);free(grid);
 }
 unsigned mh=((material_count3+1023)/1024)*2;void*m=file3(dir,"provenance-mapping.i32",1024*mh*16);
 glGenTextures(1,&c1d_mapping);glBindTexture(GL_TEXTURE_2D,c1d_mapping);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32I,1024,mh,0,GL_RGBA_INTEGER,GL_INT,m);free(m);
 glGenFramebuffers(1,&c1d_fbo);glBindFramebuffer(GL_FRAMEBUFFER,c1d_fbo);
 glGenTextures(1,&c1d_target);glBindTexture(GL_TEXTURE_2D,c1d_target);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32UI,640,480,0,GL_RGBA_INTEGER,GL_UNSIGNED_INT,NULL);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c1d_target,0);
 c1d_proof=texture3(640,480,GL_RGBA8,NULL);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,c1d_proof,0);
 c1d_texel=texture3(640,480,GL_RGBA8,NULL);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,c1d_texel,0);
 glGenRenderbuffers(1,&c1d_depth);glBindRenderbuffer(GL_RENDERBUFFER,c1d_depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,640,480);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,c1d_depth);
 GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};glDrawBuffers(3,buffers);CHECK3(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glBindFramebuffer(GL_FRAMEBUFFER,0);CHECK3(glGetError()==GL_NO_ERROR);
}
static void c1d_begin(const Header3*h){if(!c1d_debug)return;glBindFramebuffer(GL_FRAMEBUFFER,c1d_fbo);GLenum bufs[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};glDrawBuffers(3,bufs);GLuint zero[4]={0};float bg[]={h->bg[0],h->bg[1],h->bg[2],0},z=1,clear[4]={0};glClearBufferuiv(GL_COLOR,0,zero);glClearBufferfv(GL_COLOR,1,bg);glClearBufferfv(GL_COLOR,2,clear);glClearBufferfv(GL_DEPTH,0,&z);}
static void c1d_capture(unsigned tick){glBindFramebuffer(GL_READ_FRAMEBUFFER,c1d_fbo);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,640,480,GL_RGBA_INTEGER,GL_UNSIGNED_INT,c1d_pixels);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,c1d_proof_pixels);glReadBuffer(GL_COLOR_ATTACHMENT2);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,c1d_texel_pixels);glReadBuffer(GL_COLOR_ATTACHMENT0);rr_c1d_frame(tick,c1d_pixels,c1d_proof_pixels,c1d_texel_pixels);CHECK3(glGetError()==GL_NO_ERROR);}
