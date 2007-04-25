
struct intel_prim_emit_stage {
   struct prim_pipeline_stage base;

   struct intel_vb *vb;
   GLuint vert_space;

   struct {
      GLuint *elts;
      GLuint count;
      GLuint space;
   } elts;
         
   GLuint prim;
};
   

static void set_primitive( struct prim_pipeline_stage *stage,
			   GLenum primitive )
{
   struct prim_emit_stage *emit = prim_emit_stage( stage );


   if (primitive != emit->prim) {
      struct intel_render *render = stage->pipe->draw.render;

      render->set_prim( render, primitive, GL_FALSE );
      emit->prim = primitive;
   }
}

static void flush( struct prim_pipeline_stage *stage )
{
   struct prim_emit_stage *emit = prim_emit_stage( stage );
   struct intel_render *rasterizer = stage->pipe->draw->hw;
   
   /* XXX: Tweak destination vb's dirty flags so that it doesn't try
    * and go to tnl to load these vertices.  Need to break the link
    * between vb's and tnl somehow (perhaps by getting rid of tnl).
    */
   {
      emit->vb->hw.dirty = 1;
      emit->vb->local.dirty = 0;
   }

   rasterizer->set_verts( rasterizer, emit->vb );
   rasterizer->draw_indexed_prim( rasterizer, emit->elts.elts, emit->elts.count );
   
   emit->elts.space += emit->elts.count;
   emit->elts.count = 0;

   emit->vert_space = 1024;
   
   /* Clear index value on all cached vertices in the prim pipeline
    * itself.
    */
   {
      struct intel_vb *vb = emit->base.pipe->input.vb;
      GLuint i;

      for (i = 0; i < vb->nr_verts; i++) {
	 struct vertex_header *vert = intel_vb_get_vertex( vb, i );
	 vert->index = ~0;
      }
   }
}

/* Check for sufficient vertex and index space.  Return pointer to
 * index list.  
 */
static GLuint *check_space( struct prim_pipeline_stage *stage,
			    GLenum primitive,
			    GLuint nr_verts,
			    GLuint nr_elts )
{
   struct prim_emit_stage *emit = prim_emit_stage( stage );
   GLuint *ptr;

   if (primitive != emit->primitive) 
      set_primitive( emit, primitive );

   if (nr_verts >= emit->vert.space ||
       nr_elts >= emit->elts.space)
      flush( emit );

   ptr = emit->elts.ptr + emit->elts.count;
   emit->elts.count += nr_elts;
   emit->elts.space -= nr_elts;

   return ptr;
}


/* Check for vertex in buffer and emit if necessary.  Return index.
 * No need to check space this has already been done.
 */
static GLuint emit_vert( struct prim_pipeline_stage *stage,
			 struct vertex_header *vert )
{
   if (vert->index == ~0) {
      vert->index = draw->max_index++;
      memcpy( get_vert(draw->vb, vert->index), 
	      vert->data, 
	      stage->vertex_size );
   }

   return vert->index;   
}



static void emit_tri( struct prim_pipeline_stage *stage,
		      struct vertex_header *v0,
		      struct vertex_header *v1,
		      struct vertex_header *v2 )
{
   GLuint *elts = check_space( stage, GL_TRIANGLES, 3, 3 );

   elts[0] = emit_vert( stage, v0 );
   elts[1] = emit_vert( stage, v1 );
   elts[2] = emit_vert( stage, v2 );
}


static void emit_line( struct prim_pipeline_stage *stage,
		       struct vertex_header *v0,
		       struct vertex_header *v1 )
{
   GLuint *elts = check_space( stage, GL_LINES, 2, 2 );

   elts[0] = emit_vert( stage, v0 );
   elts[1] = emit_vert( stage, v1 );
}

static void emit_point( struct prim_pipeline_stage *stage,
			struct vertex_header *v0 )
{
   GLuint *elts = check_space( stage, GL_POINTS, 1, 1 );

   elts[0] = emit_vert( stage, v0 );
}


struct intel_prim_stage *intel_prim_emit_create( struct intel_prim_pipeline *pipe )
{
   
}
