/* Convert only page-group runs in the offscreen warm packet. */
static void c1_repack_warm(void){
 Slot3*s=&p1b_warm_slot;Header3*h=(Header3*)s->bytes;size_t off=sizeof *h+h->instances*sizeof(Instance3);
 Quad3*q=(Quad3*)(s->bytes+off);off+=h->n*sizeof *q;uint16_t*v=(uint16_t*)(s->bytes+off);off+=(h->visible*2+3)&~3u;
 Group3*old=(Group3*)(s->bytes+off);Group3 groups[MAXQ3];unsigned ng=0;
 for(unsigned i=0;i<h->groups;i++){Group3*g=old+i;for(unsigned j=0;j<g->count;j++){unsigned k=g->first+j,page=material_data3[q[v[k]].material].page;
 if(!j||groups[ng-1].page!=page){groups[ng]=*g;groups[ng].first=k;groups[ng].count=0;groups[ng].page=page;ng++;}groups[ng-1].count++;}}
 size_t tail=off+h->groups*sizeof(Group3),length=s->size-tail;CHECK3(off+ng*sizeof(Group3)+length<=RAW3);
 memmove(s->bytes+off+ng*sizeof(Group3),s->bytes+tail,length);memcpy(s->bytes+off,groups,ng*sizeof(Group3));s->size=off+ng*sizeof(Group3)+length;h->groups=ng;
}
