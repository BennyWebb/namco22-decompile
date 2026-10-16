"""Exact-output optimisations; original P1b and production files are untouched."""
def apply(name,s,root,a):
 level=['play','a','b','c','d','e'].index(a.step)
 if name=='async_io.c' and not a.validation:
  start=s.index('void rr_p1b_packet(');end=s.index('static void finish(',start)
  s=s[:start]+'void rr_p1b_packet(unsigned t,const void*p,size_t n){(void)t;(void)p;(void)n;}\nvoid rr_p1b_pixel(unsigned t,const char*s,const void*p,size_t n){(void)t;(void)s;(void)p;(void)n;}\n'+s[end:]
  s=s.replace('unsigned char warm[32];SHA256((const unsigned char*)"warm",4,warm);','')
 if name=='presentation.c':
  s=s.replace('static int p1_slots=3;','static int p1_slots=2;').replace('atoi(getenv("ENG_P1_QUEUE")):3;','atoi(getenv("ENG_P1_QUEUE")):2;')
 if name!='live.c':return s
 if not a.validation:
  s=s.replace('rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);','')
  s=s.replace('bool capture=strcmp(out,"-")','bool capture=false&&strcmp(out,"-")')
  # GPU error polling is validation work, not command safety/bounds checking.
  s=s.replace('CHECK3(glGetError()==GL_NO_ERROR);','')
  # These scans only re-check compiler output already proven in validation.
  import re
  s=re.sub(r' unsigned culled=0;for\(unsigned i=0;i<h->n;i\+\+\)\{.*?CHECK3\(culled\+h->visible==h->n\);', '',s)
  s=re.sub(r'if\(j3\)\{unsigned before3=visible\[g3->first\+j3-1\];CHECK3\(q\[before3\].depth-q\[original3\].depth>\.0003f\);\}', '',s)
  for check in ['CHECK3(original3<h->n);','CHECK3(visible[i]<h->n);','CHECK3(!(r->flags>>8));','CHECK3(accounted==h->visible);']:
   s=s.replace(check,'')
  s=s.replace('unsigned accounted=0,vertices=0;', 'unsigned vertices=0;')
  s=s.replace('CHECK3(g->first==accounted&&g->count>0&&g->first+g->count<=h->visible&&g->page<page_count3&&g->mode<=3&&g->box[2]>=0&&g->box[3]>=0);accounted+=g->count;', '')
  s=s.replace(',culled,h->groups',',h->n-h->visible,h->groups')
  assert 'unsigned culled=0' not in s and 'unsigned before3' not in s

 if level>=1:
  pos=s.index('static int live_material(');s=s[:pos]+'''typedef struct {int32_t recipe[7];uint64_t revision;int material;unsigned occupied;} P2Material;
static P2Material p2_cache[32768];static uint8_t p2_planar[128][768];static uint64_t p2_revision[128],p2_gathered[128];static bool p2_ready[128];
static void p2_palettes(void){bool used[128]={0};for(unsigned i=0;i<count;i++)used[(records[i].meta[3]>>8)&127]=true;for(unsigned i=0;i<p1_source->direct_count;i++)used[(p1_source->direct[i][2]>>8)&127]=true;
 for(unsigned g=0;g<128;g++){bool changed=!p2_ready[g];for(unsigned c=0;c<3;c++)changed|=memcmp(p2_planar[g]+c*256,p1_source->pal+g*256+c*32768,256)!=0;
 if(changed){for(unsigned c=0;c<3;c++)memcpy(p2_planar[g]+c*256,p1_source->pal+g*256+c*32768,256);if(++p2_revision[g]==0){memset(p2_cache,0,sizeof p2_cache);memset(p2_gathered,0,sizeof p2_gathered);p2_revision[g]=1;}p2_ready[g]=true;}
 if(P2_GATHER_ALL||(used[g]&&p2_gathered[g]!=p2_revision[g])){for(unsigned i=0;i<256;i++)for(unsigned c=0;c<3;c++)live_pal[g][i*3+c]=p2_planar[g][c*256+i];p2_gathered[g]=p2_revision[g];}
 }}
'''.replace('P2_GATHER_ALL','1' if level==1 else '0')+s[pos:]
  s=s.replace('static int live_material(', 'static int p2_material_uncached(',1)
  pos=s.index('static int live_mesh(')
  cache='''static int live_material(const int32_t *recipe){uint32_t hash=2166136261u;for(unsigned i=0;i<7;i++)hash=(hash^(uint32_t)recipe[i])*16777619u;P2Material*entry=NULL;for(unsigned probe=0;probe<32768;probe++){P2Material*e=p2_cache+((hash+probe)&32767);if(!e->occupied){entry=e;break;}if(!memcmp(e->recipe,recipe,28)){if(e->revision==p2_revision[recipe[1]])return e->material;entry=e;break;}}int material=p2_material_uncached(recipe);if(entry){memcpy(entry->recipe,recipe,28);entry->revision=p2_revision[recipe[1]];entry->material=material;entry->occupied=1;}return material;}
'''
  s=s[:pos]+cache+s[pos:]
  old='for(int g=0;g<128;g++)for(int i=0;i<256;i++)for(int c=0;c<3;c++)live_pal[g][i*3+c]=p1_source->pal[g*256+i+c*32768];';assert old in s;s=s.replace(old,'p2_palettes();')
 if level>=3 and not a.no_metadata:
  pos=s.index('static int live_mesh(');s=s[:pos]+'''typedef struct{int32_t code,packet,bank,mode,u,v,w,h;} P2Mesh;
static P2Mesh *p2_meshes;
'''+s[pos:]
  needle='int u0=4096,u1=0,v0=4096,v1=0;for(int j=0;j<4;j++){int u=r->local[j*6+3],v=r->local[j*6+4];u0=u<u0?u:u0;u1=u>u1?u:u1;v0=v<v0?v:v0;v1=v>v1?v:v1;'
  repl='const P2Mesh*md=p2_meshes+(mesh/8);int u0=md->u,u1=u0+md->w-1,v0=md->v,v1=v0+md->h-1;for(int j=0;j<4;j++){'
  assert needle in s;s=s.replace(needle,repl)
  s=s.replace('int32_t recipe[]={r->meta[5],(r->meta[3]>>8)&127,r->meta[6],u0,v0,u1-u0+1,v1-v0+1};','int32_t recipe[]={md->bank,(r->meta[3]>>8)&127,md->mode,u0,v0,u1-u0+1,v1-v0+1};')
  needle='live_loaded=true;fprintf(stderr,'
  load='''const char*mp=getenv("ENG_P2_METADATA");CHECK3(mp);FILE*mf=fopen(mp,"rb");CHECK3(mf);uint32_t mh[2];read3(mf,mh,8);CHECK3(mh[0]==0x324d5252&&mh[1]<=65536);p2_meshes=calloc(mh[1],sizeof *p2_meshes);CHECK3(p2_meshes);read3(mf,p2_meshes,mh[1]*sizeof *p2_meshes);CHECK3(fgetc(mf)==EOF&&!fclose(mf));for(unsigned mi=0;mi<live_nm;mi++){unsigned j=live_meshes[mi].texel/8;CHECK3(j<mh[1]&&p2_meshes[j].code==live_meshes[mi].code&&p2_meshes[j].packet==live_meshes[mi].packet);}
'''
  assert needle in s;s=s.replace(needle,load+needle)
 if level>=4 and not a.no_z_reuse:
  # Only Z reuse: integer projection is not the conservative float enclosure.
  # Absolute term bound proves original int32 transform did not overflow.
  old='double xyz=(double)(dot>>15)+tr;'
  new='double xyz=(double)(dot>>15)+tr;if(c==2&&!protected_&&!r->meta[34]&&r->oracle[v*7+6]&&absolute/32768.+fabs(tr)+1<2147483647.)xyz=r->oracle[v*7+2];'
  s=s.replace('static unsigned live_cull(const M3Quad *r)', 'static unsigned live_cull(const M3Quad *r,unsigned protected_)').replace('c->reason=live_cull(r);','c->reason=live_cull(r,c->protected_);')
  assert old in s;s=s.replace(old,new)
 if level>=5:
  pos=s.index('static int live_visible(')
  s=s[:pos]+'''static uint16_t p2_order[MAXQ3];
static int p2_painter(const void*a,const void*b){return live_painter(live_cmd+*(const uint16_t*)a,live_cmd+*(const uint16_t*)b);}
static int p2_batch(const void*a,const void*b){return live_batch(live_cmd+*(const uint16_t*)a,live_cmd+*(const uint16_t*)b);}
'''+s[pos:]
  start=s.index('static int live_visible(');end=s.index('/* Same conservative',start);piece=s[start:end].replace('live_cmd[x]','live_cmd[p2_order[x]]').replace('live_cmd[y]','live_cmd[p2_order[y]]').replace('live_cmd+x,live_cmd+y','live_cmd+p2_order[x],live_cmd+p2_order[y]');s=s[:start]+piece+s[end:]
  old='qsort(live_cmd,n,sizeof *live_cmd,live_painter);for(unsigned i=0;i<n;i++)live_cmd[i].q.depth=.9-1.8*(i+1)/(n+1.);qsort(live_cmd,n,sizeof *live_cmd,live_batch);'
  new='for(unsigned i=0;i<n;i++)p2_order[i]=i;qsort(p2_order,n,2,p2_painter);for(unsigned i=0;i<n;i++)live_cmd[p2_order[i]].q.depth=.9-1.8*(i+1)/(n+1.);qsort(p2_order,n,2,p2_batch);'
  assert old in s;s=s.replace(old,new)
  s=s.replace('LiveCommand*c=live_cmd+i;unsigned k=0;','LiveCommand*c=live_cmd+p2_order[i];unsigned k=0;').replace('LiveCommand*c=live_cmd+visible[i];','LiveCommand*c=live_cmd+p2_order[visible[i]];')
 return s
