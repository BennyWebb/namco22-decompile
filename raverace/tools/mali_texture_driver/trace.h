/* Private diagnostic interposition: CPU API wall time, never GPU execution. */
#include <GLES3/gl3.h>
#include <stdint.h>
void mt_bind(GLenum,GLuint);
void mt_gen(GLsizei,GLuint*);
void mt_delete(GLsizei,const GLuint*);
void mt_image2(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
void mt_sub2(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*);
void mt_image3(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
void mt_sub3(GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei,GLenum,GLenum,const void*);
void mt_reference(GLuint,int);
void mt_scope(const char*,double);
void mt_sync_object(GLuint);
void mt_invalidate(GLuint,int,unsigned);
GLenum mt_wait(GLsync,GLbitfield,GLuint64);
GLsync mt_fence(GLenum,GLbitfield);
void mt_delete_sync(GLsync);
#ifdef MT_WRAP
#define glClientWaitSync mt_wait
#define glFenceSync mt_fence
#define glDeleteSync mt_delete_sync
#define glGenTextures mt_gen
#define glDeleteTextures mt_delete
#define glTexImage2D mt_image2
#define glTexSubImage2D mt_sub2
#define glTexImage3D mt_image3
#define glTexSubImage3D mt_sub3
#endif
