from pathlib import Path

def ch(s,a,b):
 assert a in s,a[:140]
 return s.replace(a,b)
def apply(name,s,root,a):
 here=Path(__file__).parent
 if name=='rr_presentation.h':return ch(s,'    void *native_payload;', '    unsigned g1b_major;void *g1b_dynamic;\n    void *native_payload;')
 if name=='presentation.c':return ch(ch(s,'free(slots[i].native_payload);','free(slots[i].native_payload);free(slots[i].g1b_dynamic);'),'snapshot->native_tag=0; snapshot->frame','snapshot->g1b_major=(g_rr.wram[0xa048]<<8)|g_rr.wram[0xa049];snapshot->native_tag=0; snapshot->frame')
 if name!='live.c':return s
 # The build has no private include path; inline the pure allocator header.
 dynamic_header=(here/"dynamic.h").read_text()
 s=('#define G1B_VALIDATION 1\nextern unsigned char*SHA256(const unsigned char*,unsigned long,unsigned char*);\n' if a.validation else '')+s
 s=ch(s,'static GLuint pages3[16]', dynamic_header+'''

static G1BSlot*g1b_slot;static const G1BSlot*g1b_drawing;static unsigned g1b_page_index;
static GLuint g1b_pages[3],g1b_uv[3],g1b_mapping[3];static uint64_t g1b_resident[3][G1B_ENTRIES];
static int g1b_last_recipe[7],g1b_fail_code,g1b_fail_packet;static bool g1b_key_valid;
static int32_t*g1b_reference;static unsigned g1b_reference_count;
extern uint8_t texture_pen_lookup(int,int,int);
static unsigned g1b_group(unsigned);static unsigned g1b_material_page(unsigned);static void g1b_upload(const rr_presentation*,unsigned);
static GLuint pages3[16]''')
 s=s.replace('q[i].material<material_count3','q[i].material<material_count3+G1B_MATERIALS').replace('c1_group[r->material]','g1b_group(r->material)').replace('c->page=material_data3[material].page','c->page=g1b_material_page(material)')
 s=ch(s,'native_ram3=true;', 'native_ram3=getenv("ENG_G1B_STREAM_TRACE")==NULL;fprintf(stderr,"[G1B-LOG] transport=%s async_cookie=1\\n",native_ram3?"RAM-shutdown":"streaming");')
 # Allocate material resources for static data plus bounded per-picture direct IDs.
 s=ch(s,'unsigned mh=(material_count3+1023)/1024;', 'unsigned mh=(material_count3+G1B_MATERIALS+1023)/1024;')
 s=ch(s,'materials3=texture3(1024,mh,GL_RGBA32F,uv);free(uv);', 'materials3=texture3(1024,mh,GL_RGBA32F,uv);g1b_uv[0]=materials3;for(unsigned k=1;k<3;k++)g1b_uv[k]=texture3(1024,mh,GL_RGBA32F,uv);free(uv);')
 s=ch(s,'unsigned mh=((material_count3+1023)/1024)*2;', 'unsigned mh=((material_count3+G1B_MATERIALS+1023)/1024)*2;')
 # Additional page resources are fully owned by rr-present and follow its existing copy fences.
 s=ch(s,' c1d_init(c1dir);', ''' g1b_page_index=page_count3++;CHECK3(page_count3<=16);widths3[g1b_page_index]=heights3[g1b_page_index]=1024;
 for(unsigned k=0;k<3;k++){glGenTextures(1,g1b_pages+k);glBindTexture(GL_TEXTURE_2D,g1b_pages[k]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_R8UI,1024,1024,0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,NULL);}pages3[g1b_page_index]=g1b_pages[0];
 c1d_init(c1dir);
 if(c1d_debug){g1b_mapping[0]=c1d_mapping;unsigned mh=((material_count3+G1B_MATERIALS+1023)/1024)*2;void*m=file3(c1dir,"provenance-mapping.i32",1024*mh*16);for(unsigned k=1;k<3;k++){glGenTextures(1,g1b_mapping+k);glBindTexture(GL_TEXTURE_2D,g1b_mapping[k]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32I,1024,mh,0,GL_RGBA_INTEGER,GL_INT,m);}free(m);}
 fprintf(stderr,"[G1B] dynamic_page=1024 copies=3 slots=owned allocator=LRU-grid8 ROM_decoder=texture_pen_lookup material_cache=exact-key activation=RAM\\n");''')
 # Keep track-selection course distinct from the changing route/section word.
 s=s.replace('course=%u walk=%u','course=%u major=%u walk=%u').replace('s->g1_course,s->walk','s->g1_course,s->g1b_major,s->walk')
 s=ch(s,'static void g1_receipt(rr_presentation*s,const char*why){fprintf', 'static void g1_receipt(rr_presentation*s,const char*why){const G1BSlot*d=s->g1b_dynamic;if(d&&d->tick==s->frame)fprintf(stderr,"[G1B-PREP] tick=%u hits=%u misses=%u evictions=%u pending_bytes=%u\\n",s->frame,d->hits,d->misses,d->evictions,d->bytes);fprintf')
 # Runtime decoding uses existing immutable texture ROMs, never fopen or GL on preparer.
 code=(here/'dynamic.inc').read_text();start=code.index('static uint8_t g1b_pen');s=ch(s,'static void live_compile(void){', code[start:]+'\nstatic void live_compile(void){')
 s=ch(s,'static void live_fail(const char *why,int code,int packet){', 'static void live_fail(const char *why,int code,int packet){g1b_fail_code=code;g1b_fail_packet=packet;')
 s=ch(s,'int material=live_material(recipe);if(material<0)live_fail("direct material/palette absent from immutable M3 pack",-1,i);', 'int material=live_material(recipe);if(material<0)material=g1b_material(recipe);')
 s=ch(s,'int u0=4096,u1=0,v0=4096,v1=0;for(int j=0;j<4;j++)', 'int u0=4096,u1=0,v0=4096,v1=0;for(int j=0;j<4;j++)') # assert known static branch retained
 s=ch(s,'p1_output=s->native_payload;', '''p1_output=s->native_payload;
 if(!s->g1b_dynamic){s->g1b_dynamic=calloc(1,sizeof(G1BSlot));if(!s->g1b_dynamic)return false;}g1b_slot=s->g1b_dynamic;memset(g1b_slot,0,offsetof(G1BSlot,pens));g1b_slot->tick=s->frame;g1b_key_valid=false;g1b_fail_code=g1b_fail_packet=-1;''')
 s=ch(s,'g1_fallback++;g1_receipt(s,p1_reason);', '''g1_fallback++;g1_receipt(s,p1_reason);fprintf(stderr,"[G1B-FALLBACK] tick=%u code=%d packet=%d key_valid=%u recipe=%d,%d,%d,%d,%d,%d,%d reason=%s\\n",s->frame,g1b_fail_code,g1b_fail_packet,g1b_key_valid,g1b_last_recipe[0],g1b_last_recipe[1],g1b_last_recipe[2],g1b_last_recipe[3],g1b_last_recipe[4],g1b_last_recipe[5],g1b_last_recipe[6],p1_reason);''')
 s=ch(s,'instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;', 'g1b_upload(s,i);instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;')
 # Validation-only adjusted identities; lookup populated from runtime keys, never a catalogue addition.
 if a.validation:
  s=ch(s,'P1_VALID(g1_ref_material[material]>=0);c1_copy[k].q.material=g1_ref_material[material];', 'int ref=material<(int)material_count3?g1_ref_material[material]:g1b_reference_id(g1b_slot->material[material-material_count3].recipe);P1_VALID(ref>=0);c1_copy[k].q.material=ref;')
 s=ch(s,'read3(rf,g1_ref_material,rh[1]*4);CHECK3(fgetc(rf)==EOF', 'read3(rf,g1_ref_material,rh[1]*4);read3(rf,&g1b_reference_count,4);CHECK3(g1b_reference_count<=65535);g1b_reference=malloc(g1b_reference_count*32);CHECK3(g1b_reference);read3(rf,g1b_reference,g1b_reference_count*32);CHECK3(fgetc(rf)==EOF')
 s=s.replace(';int material=live_material(recipe);', ';memcpy(g1b_last_recipe,recipe,28);g1b_key_valid=true;int material=live_material(recipe);')
 # Emit dynamic ID-to-recipe attribution only for sampled validation pictures.
 if a.validation:
  s=ch(s,'live_compile();if(c1_index)', '''live_compile();if(getenv("ENG_C1_IMAGES")&&(s->frame==1800||s->frame==1965||s->frame==2500||s->frame==2643||s->frame==3000||s->frame==3546||s->frame==3600||s->frame==4200||s->frame==4800||s->frame==5400||s->frame==5639||s->frame==5655||s->frame==5867||s->frame==5874||s->frame==5878||s->frame==5889||s->frame==5921||s->frame==6006)){for(unsigned j=0;j<g1b_slot->materials;j++){int*r=g1b_slot->material[j].recipe;fprintf(stderr,"[G1B-MATERIAL] tick=%u id=%u recipe=%d,%d,%d,%d,%d,%d,%d\\n",s->frame,material_count3+j,r[0],r[1],r[2],r[3],r[4],r[5],r[6]);}}if(c1_index)''')
 return s
