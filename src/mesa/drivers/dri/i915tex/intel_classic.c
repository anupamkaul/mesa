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

 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */
      
#include "intel_context.h"
#include "intel_vb.h"
#include "intel_batchbuffer.h"
#include "intel_reg.h"
#include "intel_buffers.h"
#include "intel_state.h"
#include "intel_draw.h"


struct classic_render {
   struct intel_render render;
   struct intel_context *intel;
   GLuint hw_prim;
   GLuint offset;
};

static INLINE struct classic_render *classic_render( struct intel_render *render )
{
   return (struct classic_render *)render;
}


static void classic_new_vertices( struct intel_render *render )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;

   GLboolean ok = intel_vb_validate_vertices( intel->vb, VB_HW_VERTS );

   if (!ok) {
      intel->render->flush( intel->render, GL_FALSE );

      /* Not really sure how this could fail: 
       */
      ok = intel_vb_validate_vertices( intel->vb, VB_HW_VERTS );
      assert(ok);
   }

   crc->offset = intel_vb_get_vbo_index_offset( intel->vb );
}


static void classic_draw_prim_indexed( struct intel_render *render,
				       const GLuint *indices,
				       GLuint nr )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;
   const GLuint offset = crc->offset;
   GLuint j;

   /* The 'dwords' usage below ensures that both the state and the
    * primitive command below end up in the same batchbuffer,
    * otherwise there is a risk that another context might
    * interpose a batchbuffer containing different statesetting
    * commands.
    */
   GLuint dwords = 1 + (nr+1)/2;
   intel_emit_hardware_state(intel, dwords);


   /* XXX: Can emit upto 64k indices, need to split larger prims
    */
   BEGIN_BATCH( dwords, INTEL_BATCH_CLIPRECTS );
   OUT_BATCH( _3DPRIMITIVE | 
	      crc->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_ELTS | 
	      nr );
      
   /* Pack indices into 16bits 
    */
   for (j = 0; j < nr-1; j += 2) {
      OUT_BATCH( (offset + indices[j]) | ((offset + indices[j+1])<<16) );
   }

   if (j < nr) {
      OUT_BATCH( (offset + indices[j]) );
   }
 
   ADVANCE_BATCH();
}


static void classic_draw_prim( struct intel_render *render,
			       GLuint start,
			       GLuint nr )
{
   struct classic_render *crc = classic_render( render );
   struct intel_context *intel = crc->intel;
   GLuint dwords = 2;

   intel_emit_hardware_state(intel, dwords);

   BEGIN_BATCH( dwords, INTEL_BATCH_CLIPRECTS );
   OUT_BATCH( _3DPRIMITIVE | 
	      crc->hw_prim | 
	      PRIM_INDIRECT | 
	      PRIM_INDIRECT_SEQUENTIAL | 
	      nr );      
   OUT_BATCH( crc->offset + start );
   ADVANCE_BATCH();
}

static void classic_set_prim( struct intel_render *render,
			      GLuint hw_prim )
{
   struct classic_render *crc = classic_render( render );

   _mesa_printf( "%s %x\n", __FUNCTION__, hw_prim);
   crc->hw_prim = hw_prim;
}


static void classic_start_render( struct intel_render *render )
{
   struct classic_render *crc = classic_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   /* Start a new batchbuffer, emit wait for pending flip.
    */
   intel_wait_flips(crc->intel, 0);
}


static void classic_flush( struct intel_render *render, 
			   GLboolean finished_frame )
{
   struct classic_render *crc = classic_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   struct intel_context *intel = crc->intel;

   if (intel->batch->segment_finish_offset[0] != 0)
      intel_batchbuffer_flush(intel->batch);
}

static void classic_abandon_frame( struct intel_render *render )
{
   /* struct classic_render *crc = classic_render( render ); */
   _mesa_printf("%s\n", __FUNCTION__);
}

static void classic_clear( struct intel_render *render )
{
   /* Not really sure how this will work yet. 
    */
   /* struct classic_render *crc = classic_render( render ); */
   _mesa_printf("%s\n", __FUNCTION__);
   assert(0);
}



static void classic_destroy_context( struct intel_render *render )
{
   struct classic_render *crc = classic_render( render );
   _mesa_printf("%s\n", __FUNCTION__);

   _mesa_free(crc);
}

struct intel_render *intel_create_classic_render( struct intel_context *intel )
{
   struct classic_render *crc = CALLOC_STRUCT(classic_render);

   /* XXX: Add casts here to avoid the compiler messages:
    */
   crc->render.destroy_context = classic_destroy_context;
   crc->render.start_render = classic_start_render;
   crc->render.abandon_frame = classic_abandon_frame;
   crc->render.flush = classic_flush;
   crc->render.clear = classic_clear;
   crc->render.set_prim = classic_set_prim;
   crc->render.new_vertices = classic_new_vertices;
   crc->render.draw_prim = classic_draw_prim;
   crc->render.draw_indexed_prim = classic_draw_prim_indexed;

   crc->intel = intel;
   crc->hw_prim = 0;

   return &crc->render;
}
