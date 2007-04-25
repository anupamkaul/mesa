#include "intel_prim.h"

struct twoside_stage {
   struct prim_pipeline_stage base;
   
   GLboolean facing_flag;

   GLuint col0_offset;
   GLuint bfc0_offset;

   GLuint col1_offset;
   GLuint bfc1_offset;
};

struct prim_pipeline_stage *intel_prim_twoside( struct prim_pipeline *pipe )
{
   struct twoside_stage *twoside = MALLOC(sizeof(*twoside));

   if (stage) {
      prim_init_stage( pipe, &twoside->base, 2 );

      twoside->base.tri = twoside_tri;
   }

   return stage;
}


static void copy_bfc( struct twoside_stage *twoside, 
		      struct vertex_header *v )
{
   memcpy( v->data + twoside->col0_offset, 
	   v->data + twoside->bfc0_offset, 
	   sizeof(GLuint) );

   if (twoside->col1_offset) 
      memcpy( v->data + twoside->col1_offset, 
	      v->data + twoside->bfc1_offset, 
	      sizeof(GLuint) );
}


/* Twoside tri:
 */
static void twoside_tri( struct prim_pipeline_stage *stage,
			  struct vertex_header *v0,
			  struct vertex_header *v1,
			  struct vertex_header *v2 )
{
   GLuint det = calc_det(v0, v1, v2);
   GLbooean backface = (det < 0) ^ twoside->facing_flag;
  
   if (backface) {
      v0 = dup_vert(stage, v0);
      v1 = dup_vert(stage, v1);
      v2 = dup_vert(stage, v2);
      
      copy_bfc(stage, v0);
      copy_bfc(stage, v1);
      copy_bfc(stage, v2);
   }

   stage->next->tri( stage->next, v0, v1, v2 );
}
