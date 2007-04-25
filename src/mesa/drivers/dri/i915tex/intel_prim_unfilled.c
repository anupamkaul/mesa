struct prim_unfilled_stage {
   struct prim_pipeline_stage base;

   GLenum mode[2];
};



/* Unfilled tri:  
 *
 * Note edgeflags in the vertex struct is not sufficient as we will
 * need to manipulate them when decomposing primitives???
 */
static void unfilled_tri( struct prim_pipeline_stage *stage,
			  struct vertex_header *v0,
			  struct vertex_header *v1,
			  struct vertex_header *v2 )
{
   GLuint det = calc_det(v0, v1, v2);
   GLenum mode = draw->unfilled_mode[det < 0];
   struct prim_pipeline_stage *next = stage->next;
  
   switch (mode) {
   case GL_FILL:
      next->tri( next, v0, v1, v2 );
      break;

   case GL_LINE:
      if (ef0) next->line( next, v0, v1 );
      if (ef1) next->line( next, v1, v2 );
      if (ef2) next->line( next, v2, v0 );
      break;

   case GL_POINT:
      if (ef0) next->point( next, v0 );
      if (ef1) next->point( next, v1 );
      if (ef2) next->point( next, v2 );
      break;
   }   
}
