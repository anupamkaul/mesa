#include "intel_prim.h"

struct prim_pipeline_stage *intel_prim_flatshade( struct prim_pipeline *pipe )
{
   struct prim_pipeline_stage *stage = MALLOC(sizeof(*stage));

   if (stage) {
      prim_init_stage( pipe, stage, 2 );

      stage->tri = flatshade_tri;
      stage->line = flatshade_line;
   }

   return stage;
}


#define COLOR_BITS ((1 << VF_BIT_COLOR0) |	\
		    (1 << VF_BIT_COLOR1) |	\
		    (1 << VF_BIT_BFC0) |	\
		    (1 << VF_BIT_BFC1))

static void copy_colors( struct vertex_fetch *vf, 
			 GLubyte *vdst, 
			 const GLubyte *vsrc )
{
   const struct vertex_fetch_attr *a = vf->attr;
   const GLuint attr_count = vf->attr_count;
   GLuint j;

   for (j = 0; j < attr_count; j++) {
      if ((1 << a[j].attrib) & COLOR_BITS) {
	 memcpy( vdst + a[j].vertoffset,
		 vsrc + a[j].vertoffset,
		 a[j].vertattrsize );
      }
   }
}



/* Flatshade tri.  Required for clipping and when unfilled tris are
 * active, otherwise handled by hardware.
 */
static void flatshade_tri( struct prim_pipeline_stage *stage,
			   struct vertex_header *v0,
			   struct vertex_header *v1,
			   struct vertex_header *v2 )
{
   v0 = dup_vert(stage, v0);
   v1 = dup_vert(stage, v1);

   copy_colors(vf, v0, v2);
   copy_colors(vf, v1, v2);
   
   stage->next->tri( stage->next, v0, v1, v2 );
}


/* Flatshade line.  Required for clipping.
 */
static void flatshade_tri( struct prim_pipeline_stage *stage,
			   struct vertex_header *v0,
			   struct vertex_header *v1 )
{
   v0 = dup_vert(stage, v0);

   vf_copy_colors(vf, v0, v1);
   
   stage->next->tri( stage->next, v0, v1 );
}



