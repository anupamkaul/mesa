
/* Various helpers.  The biggest issue with this design is that
 * non-triangle primitives can end up jumping through a lot of noop
 * functions before they reach the emit stage.  May need to build &
 * tear down the pipeline on a per-reduced-primitive basis?
 */
static void nop_tri( struct prim_pipeline_stage *stage,
		     struct vertex_header *v0,
		     struct vertex_header *v1,
		     struct vertex_header *v2 )
{
   stage = stage->next;
   stage->tri( stage, v0, v1, v2 );
}

static void nop_line( struct prim_pipeline_stage *stage,
		      struct vertex_header *v0,
		      struct vertex_header *v1 )
{
   stage = stage->next;
   stage->line( stage, v0, v1 );
}

static void nop_point( struct prim_pipeline_stage *stage,
		       struct vertex_header *v0 )
{
   stage = stage->next;
   stage->point( stage, v0 );
}


void prim_stage_init( struct prim_pipeline_stage *stage,
		      struct prim_pipeline *pipe,
		      GLuint nr_temps )
{
   clip->base.pipe = pipe;
   clip->base.next = NULL;
   clip->base.tmp = TRY_MALLOC(MAX_CLIPPED_VERTICES * sizeof(clip->base.tmp[0]), out2);

   for (i = 0; i < MAX_CLIPPED_VERTICES; i++)
      clip->base.tmp[i] = TRY_MALLOC(INTEL_MAX_VERTEX_SIZE, out4);

   clip->base.point = nop_point;
   clip->base.line = nop_line;
   clip->base.tri = nop_tri;

   return &clip->base;

 out4:
   for (i = 0; i < MAX_CLIPPED_VERTICES; i++)
      if (clip->base.tmp[i])
	 FREE(clip->base.tmp[i]);
   FREE(clip->base.tmp);
 out2:
   FREE(clip);
 out1:
   return NULL;
}


void prim_stage_free( struct prim_pipeline_stage *stage )
{
   GLuint i;

   for (i = 0; i < stage->nr_tmp; i++)
      FREE(stage->tmp[i]);
   FREE(stage->tmp);
   FREE(stage);
}


