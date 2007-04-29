/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */
#include "imports.h"

#define INTEL_DRAW_PRIVATE
#include "intel_draw.h"

#define INTEL_PRIM_PRIVATE
#include "intel_prim.h"

/* Don't want these too large as there is no mechanism to "give back"
 * unused space.  FIXME.
 */
#define EMIT_MAX_ELTS  1024
#define EMIT_MAX_VERTS 256

struct emit_stage {
   struct prim_stage stage;

   struct {
      GLubyte *buf;
      GLuint count;
      GLuint space;
   } verts;

   struct {
      GLuint elts[EMIT_MAX_ELTS];
      GLuint count;
      GLuint space;
   } elts;
         
   struct intel_render *hw;
   GLuint hw_vertex_size;
   GLuint hw_data_offset;
   GLuint hw_prim;
};
   

static INLINE struct emit_stage *emit_stage( struct prim_stage *stage )
{
   return (struct emit_stage *)stage;
}


static void set_primitive( struct emit_stage *emit,
			   GLenum primitive )
{
   struct intel_render *hw = emit->hw;

   if (emit->elts.count) {
      hw->draw_indexed_prim( hw, emit->elts.elts, emit->elts.count );
      emit->elts.space = EMIT_MAX_ELTS;
      emit->elts.count = 0;
   }

   hw->set_prim( hw, primitive );
   emit->hw_prim = primitive;

}

static void flush( struct emit_stage *emit, 
		   GLboolean allocate_new_vertices )
{
   struct intel_render *hw = emit->hw;
   GLboolean flush_hw = (emit->verts.buf != NULL);
   
   if (flush_hw) {
      if (emit->elts.count) {
	 GLuint i;
	 for (i = 0; i < emit->elts.count; i++)
	    assert(emit->elts.elts[i] < emit->verts.count);

	 hw->draw_indexed_prim( hw, emit->elts.elts, emit->elts.count );
	 emit->elts.space = EMIT_MAX_ELTS;
	 emit->elts.count = 0;
      }

      hw->release_vertices( hw, emit->verts.buf );    
      emit->verts.buf = NULL;
      emit->verts.count = 0;
      emit->verts.space = 0;
   }

   
   /* Clear index value on all cached vertices in the prim pipeline
    * itself.
    */
   if (allocate_new_vertices)
   {
      emit->verts.buf = hw->allocate_vertices( hw, emit->hw_vertex_size, EMIT_MAX_VERTS );
      emit->verts.space = EMIT_MAX_VERTS;
      emit->verts.count = 0;
   }

   if (flush_hw && allocate_new_vertices)
      intel_prim_reset_vertex_indices( emit->stage.pipe );
}

/* Check for sufficient vertex and index space.  Return pointer to
 * index list.  
 */
static GLuint *check_space( struct emit_stage *emit,
			    GLenum primitive,
			    GLuint nr_verts,
			    GLuint nr_elts )
{
   GLuint *ptr;

   if (primitive != emit->hw_prim) 
      set_primitive( emit, primitive );

   /* XXX: No need to discard the vertex buffer when we run out of
    * element space.
    */
   if (nr_verts >= emit->verts.space ||
       nr_elts >= emit->elts.space) 
      flush( emit, GL_TRUE );

   ptr = emit->elts.elts + emit->elts.count;
   emit->elts.count += nr_elts;
   emit->elts.space -= nr_elts;

   return ptr;
}


/* Check for vertex in buffer and emit if necessary.  Return index.
 * No need to check space this has already been done.
 */
static GLuint emit_vert( struct emit_stage *emit,
			 struct vertex_header *vert )
{
   if (vert->index == 0xffff) {
      GLuint idx = emit->verts.count;

      emit->verts.count++;
      emit->verts.space--;
      
      assert(idx < EMIT_MAX_VERTS);
      vert->index = idx;

      memcpy( emit->verts.buf + idx * emit->hw_vertex_size, 
	      vert->data + emit->hw_data_offset, 
	      emit->hw_vertex_size );
   }

   return vert->index;   
}


static void emit_begin( struct prim_stage *stage )
{
   struct emit_stage *emit = emit_stage( stage );

   /* Validate hw_vertex_size, hw_data_offset, etc:
    */
   emit->hw = stage->pipe->draw->hw;
   emit->hw_vertex_size = stage->pipe->draw->hw_vertex_size;

   if (stage->pipe->draw->vb_state.clipped_prims)
      emit->hw_data_offset = 16;
   else
      emit->hw_data_offset = 0;	

   emit->hw->set_prim( emit->hw, emit->hw_prim );

   flush( emit, GL_TRUE );
}




static void emit_tri( struct prim_stage *stage,
		      struct prim_header *header )
{
   struct emit_stage *emit = emit_stage( stage );
   GLuint *elts = check_space( emit, GL_TRIANGLES, 3, 3 );

   elts[0] = emit_vert( emit, header->v[0] );
   elts[1] = emit_vert( emit, header->v[1] );
   elts[2] = emit_vert( emit, header->v[2] );
}


static void emit_line( struct prim_stage *stage,
		       struct prim_header *header )
{
   struct emit_stage *emit = emit_stage( stage );
   GLuint *elts = check_space( emit, GL_LINES, 2, 2 );

   elts[0] = emit_vert( emit, header->v[0] );
   elts[1] = emit_vert( emit, header->v[1] );
}


static void emit_point( struct prim_stage *stage,
			struct prim_header *header )
{
   struct emit_stage *emit = emit_stage( stage );
   GLuint *elts = check_space( emit, GL_POINTS, 1, 1 );

   elts[0] = emit_vert( emit, header->v[0] );
}

static void emit_reset_tmps( struct prim_stage *stage )
{
}

static void emit_end( struct prim_stage *stage )
{
   struct emit_stage *emit = emit_stage( stage );

   flush( emit, GL_FALSE );
   emit->hw = NULL;
}

struct prim_stage *intel_prim_emit( struct prim_pipeline *pipe )
{
   struct emit_stage *emit = CALLOC_STRUCT(emit_stage);

   intel_prim_alloc_tmps( &emit->stage, 0 );

   emit->stage.pipe = pipe;
   emit->stage.next = NULL;
   emit->stage.begin = emit_begin;
   emit->stage.point = emit_point;
   emit->stage.line = emit_line;
   emit->stage.tri = emit_tri;
   emit->stage.reset_tmps = emit_reset_tmps;
   emit->stage.end = emit_end;

   return &emit->stage;
}
