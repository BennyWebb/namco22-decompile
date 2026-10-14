/* Isolated GLES2 sampler/compiler probe. Build with SDL2 + GLESv2 and run on Mali. */
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 480, TW = 64, TH = 64 };
static const char *vs =
        "#version 100\n"
        "attribute vec3 a_position; attribute vec4 a_colour; attribute vec3 a_uvq;"
        "attribute float a_texture_slot; uniform mat4 u_projection;"
        "varying mediump vec4 v_colour; varying highp vec3 v_uvq;"
        "varying mediump float v_texture_slot;"
        "void main() { gl_Position = u_projection * vec4(a_position, 1.0);"
        " v_colour = a_colour; v_uvq = a_uvq; v_texture_slot = a_texture_slot; }\n";
static const char *vs_single =
        "#version 100\n"
        "attribute vec3 a_position;\n"
        "attribute vec4 a_colour;\n"
        "attribute vec3 a_uvq;\n"
        "uniform mat4 u_projection;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { gl_Position = u_projection * vec4(a_position, 1.0);"
        " v_colour = a_colour; v_uvq = a_uvq; }\n";
static const char *fs_single =
        "#version 100\nprecision highp float;\n"
        "uniform sampler2D u_texture;uniform bool u_priority;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { vec2 uv = v_uvq.xy / v_uvq.z;"
        " vec4 texel = texture2D(u_texture, uv);"
        " if (texel.a == 0.0) discard; vec4 c=v_colour*texel; gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c; }\n";
static const char *fs_affine =
 "#version 100\nprecision highp float;uniform sampler2D u_texture;uniform bool u_priority;"
 "uniform vec4 u_region;varying mediump vec4 v_colour;varying highp vec3 v_uvq;"
 "void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec2 bounded=clamp(uv,vec2(0.5)/u_region.zw,(u_region.zw-0.5)/u_region.zw);"
 "vec4 texel=texture2D(u_texture,(u_region.xy+bounded*u_region.zw)/2048.0);"
 "if(texel.a==0.0)discard;vec4 c=v_colour*texel;gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c;}";
static const char *fs_integer =
 "#version 100\nprecision highp float;uniform sampler2D u_texture;uniform bool u_priority;"
 "uniform vec4 u_region;varying mediump vec4 v_colour;varying highp vec3 v_uvq;"
 "void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec2 pixel=clamp(floor(uv*u_region.zw),vec2(0.0),u_region.zw-1.0);"
 "vec4 texel=texture2D(u_texture,(u_region.xy+pixel+0.5)/2048.0);"
 "if(texel.a==0.0)discard;vec4 c=v_colour*texel;gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c;}";
static const char *fs_integer_uvcheck =
 "#version 100\nprecision highp float;uniform sampler2D u_texture;uniform bool u_priority;"
 "uniform vec4 u_region;varying mediump vec4 v_colour;varying highp vec3 v_uvq;"
 "void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec2 pixel=clamp(floor(uv*u_region.zw),vec2(0.0),u_region.zw-1.0);"
 "vec4 texel=texture2D(u_texture,(pixel+0.5)/u_region.zw);"
 "if(texel.a==0.0)discard;vec4 c=v_colour*texel;gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c;}";
static float region[4];
static GLuint shader(GLenum type, const char *source) {
    GLuint s=glCreateShader(type);GLint ok=0;glShaderSource(s,1,&source,NULL);
    glCompileShader(s);glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof log,NULL,log);fprintf(stderr,"shader: %s\n",log);exit(2);}
    return s;
}
static GLuint program(const char *vertex_source, const char *fs) {
    GLuint v=shader(GL_VERTEX_SHADER,vertex_source),f=shader(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);
    glBindAttribLocation(p,0,"a_position");glBindAttribLocation(p,1,"a_colour");
    glBindAttribLocation(p,2,"a_uvq");glBindAttribLocation(p,3,"a_texture_slot");
    glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096];glGetProgramInfoLog(p,sizeof log,NULL,log);fprintf(stderr,"link: %s\n",log);exit(2);}
    glDeleteShader(v);glDeleteShader(f);return p;
}
typedef struct {float pos[3],colour[4],uvq[3],slot;} vertex;
#ifndef CASE_VERTICES_HEADER
#define CASE_VERTICES_HEADER "../shader_equivalence_case/vertices-high.h"
#endif
#include CASE_VERTICES_HEADER
static void draw(GLuint p, GLuint textures[4], int same_unit, int slot,
                 int priority, uint8_t *pixels) {
    const GLfloat projection[16]={2.f/W,0,0,0, 0,-2.f/H,0,0, 0,0,1,0, -1,1,0,1};
    /* Perspective UVs approach many nearest-texel boundaries; each vertex
       has a different colour and q, matching the real shader's operations. */
    glViewport(0,0,W,H);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glClearDepthf(1);glEnable(GL_SCISSOR_TEST);glScissor(0,0,W,H);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"u_projection"),1,GL_FALSE,projection);
    glUniform1i(glGetUniformLocation(p,"u_priority"),priority);
    glUniform4fv(glGetUniformLocation(p,"u_region"),1,region);
    glUniform1i(glGetUniformLocation(p,"u_texture"),0);
    for(int i=0;i<4;i++){
        char name[16];snprintf(name,sizeof name,"u_tex%d",i);
        glUniform1i(glGetUniformLocation(p,name),same_unit?0:i);
        glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);
    }
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(vertex),&vertices[0].pos);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(vertex),&vertices[0].colour);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(vertex),&vertices[0].uvq);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(vertex),&vertices[0].slot);
    for(int i=0;i<4;i++)glEnableVertexAttribArray(i);
    glDrawArrays(GL_TRIANGLES,0,sizeof(vertices)/sizeof(vertices[0]));
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glFinish();
}
static void compare(const char *name,const uint8_t *base,const uint8_t *candidate){
    unsigned pixels=0,count[4]={0},max[4]={0};uint64_t total[4]={0};
    for(size_t i=0;i<(size_t)W*H;i++){
        int changed=0;
        for(int c=0;c<4;c++){
            int d=(int)candidate[4*i+c]-base[4*i+c];
            if(d<0)d=-d;
            if(d){changed=1;count[c]++;total[c]+=(unsigned)d;if((unsigned)d>max[c])max[c]=(unsigned)d;}
        }
        pixels+=(unsigned)changed;
    }
    printf("%s: pixels=%u RGBA count=%u,%u,%u,%u max=%u,%u,%u,%u mean_affected=%.5f,%.5f,%.5f,%.5f\n",
        name,pixels,count[0],count[1],count[2],count[3],max[0],max[1],max[2],max[3],
        count[0]?(double)total[0]/count[0]:0,count[1]?(double)total[1]/count[1]:0,
        count[2]?(double)total[2]/count[2]:0,count[3]?(double)total[3]/count[3]:0);
}
int main(int argc, char **argv){
    if(SDL_Init(SDL_INIT_VIDEO)){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 2;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_Window *window=SDL_CreateWindow("sampler probe",0,0,W,H,SDL_WINDOW_OPENGL);
    if(!window){fprintf(stderr,"window: %s\n",SDL_GetError());return 2;}
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if(!context){fprintf(stderr,"context: %s\n",SDL_GetError());return 2;}
    printf("GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    FILE *tf=fopen(argc>1?argv[1]:"texture-high.rgba","rb");if(!tf)return 2;
    unsigned tw,th,uw,uh;fscanf(tf,"%u %u %u %u",&tw,&th,&uw,&uh);fgetc(tf);
    uint8_t *texels=malloc((size_t)tw*th*4);if(!texels)return 2;
    if(fread(texels,4,(size_t)tw*th,tf)!=(size_t)tw*th)return 2;fclose(tf);
    GLuint texture[4];glGenTextures(4,texture);
    for(int i=0;i<4;i++){
        glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,texture[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,tw,th,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    }
    GLuint target,framebuffer;glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"incomplete FBO\n");return 2;}
    GLuint depth;glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 2;
    GLuint programs[4]={program(vs_single,fs_single),program(vs_single,fs_affine),program(vs_single,fs_integer),program(vs_single,fs_integer_uvcheck)};
    size_t n=(size_t)W*H*4;uint8_t *base=malloc(n),*candidate=malloc(n);if(!base||!candidate)return 2;
    GLuint atlas;glGenTextures(1,&atlas);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlas);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2048,2048,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    const int origins[][2]={{0,0},{17,31},{509,997},{1300,1900}};
    for(int encoding=0;encoding<2;encoding++){
      if(encoding)for(unsigned y=0;y<th;y++)for(unsigned x=0;x<tw;x++){
        size_t at=((size_t)y*tw+x)*4;texels[at]=x&255;texels[at+1]=x>>8;texels[at+2]=y;texels[at+3]=255;
      }
      for(int i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,texture[i]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,tw,th,GL_RGBA,GL_UNSIGNED_BYTE,texels);}
      draw(programs[0],texture,1,0,1,base);char filename[160];snprintf(filename,sizeof filename,"baseline-%d.rgba",encoding);
      FILE *f=fopen(filename,"wb");fwrite(base,1,n,f);fclose(f);
      region[0]=region[1]=0;region[2]=tw;region[3]=th;
      draw(programs[3],texture,1,0,1,candidate);compare(encoding?"quantizer-ID":"quantizer-RGBA",base,candidate);
      snprintf(filename,sizeof filename,"quantizer-%d.rgba",encoding);f=fopen(filename,"wb");fwrite(candidate,1,n,f);fclose(f);
      for(int origin=0;origin<4;origin++){
        region[0]=origins[origin][0];region[1]=origins[origin][1];region[2]=tw;region[3]=th;
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlas);glTexSubImage2D(GL_TEXTURE_2D,0,origins[origin][0],origins[origin][1],tw,th,GL_RGBA,GL_UNSIGNED_BYTE,texels);
        GLuint atlases[4]={atlas,atlas,atlas,atlas};
        for(int variant=1;variant<=2;variant++){
          draw(programs[variant],atlases,1,0,1,candidate);
          char label[80];snprintf(label,sizeof label,"%s-%s-origin-%d",variant==1?"affine":"integer",encoding?"ID":"RGBA",origin);compare(label,base,candidate);
          snprintf(filename,sizeof filename,"%s.rgba",label);f=fopen(filename,"wb");fwrite(candidate,1,n,f);fclose(f);
        }
      }
    }
    GLenum error=glGetError();if(error){fprintf(stderr,"GL error: 0x%x\n",error);return 2;}
    free(base);free(candidate);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    return 0;
}
