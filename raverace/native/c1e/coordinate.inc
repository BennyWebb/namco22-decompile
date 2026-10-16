/* Nearest filtering is separable. Match each source axis's physical size and
 * sampler type/precision, with one 1D-in-2D coordinate texture per axis. */
static GLuint c1e_ygrids[16];
static GLuint c1e_axis(unsigned n,bool vertical){
 uint8_t bytes[4096*4];for(unsigned x=0;x<n;x++){bytes[x*4]=x;bytes[x*4+1]=x>>8;bytes[x*4+2]=0;bytes[x*4+3]=0;}
 GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,c1_index?GL_RGBA8UI:GL_RGBA8,vertical?1:n,vertical?n:1,0,c1_index?GL_RGBA_INTEGER:GL_RGBA,GL_UNSIGNED_BYTE,bytes);return texture;
}
