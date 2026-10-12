/* Same exact key order and record payload, including both tie modes and reuse.
 * Compile with -Iengine, with/without RR_GEOMETRY_INDEX_ORDER. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quad_gl.h"
float g_scene_x0=0, g_scene_x1=640;
#include "quad_geometry.inc"
static geo_quad source[257], work[257];
static int expected[257], tie;
static int compare(const void *a,const void *b)
{
 int i=*(const int *)a,j=*(const int *)b;
 if(source[i].zsort!=source[j].zsort) return source[i].zsort>source[j].zsort?-1:1;
 if(source[i].order==source[j].order)return 0;
 return tie?(source[i].order<source[j].order?-1:1):(source[i].order>source[j].order?-1:1);
}
int main(void)
{
 const int sizes[]={0,1,2,7,257,1,0,256,17,2};
 for(int trial=0;trial<100;trial++)for(tie=0;tie<2;tie++)for(unsigned s=0;s<sizeof sizes/sizeof *sizes;s++){
  int n=sizes[s];
  for(int i=0;i<n;i++){
   memset(&source[i],(i*31+trial)&255,sizeof source[i]);
   source[i].order=(i*37)%257;source[i].zsort=(i*19+trial)%11;
   expected[i]=i;
  }
  memcpy(work,source,(size_t)n*sizeof *work);qsort(expected,n,sizeof *expected,compare);
  eng_quad_sort(work,n,tie);
  for(int i=0;i<n;i++)assert(!memcmp(&work[eng_quad_index(i)],&source[expected[i]],sizeof *work));
#ifdef RR_GEOMETRY_INDEX_ORDER
  assert(!memcmp(work,source,(size_t)n*sizeof *work));
#endif
 }
 puts("both tie modes, full payloads, ties, empty/singleton and changing capacities passed");
}
