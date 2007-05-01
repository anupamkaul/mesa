/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "intel_context.h"
#include "intel_state.h"
#include "intel_batchbuffer.h"
#include "mtypes.h"

/***********************************************************************
 */

static GLboolean check_state( const struct intel_state_flags *a,
			      const struct intel_state_flags *b )
{
   return ((a->mesa & b->mesa) ||
	   (a->intel & b->intel) ||
	   (a->extra & b->extra));
}

static void accumulate_state( struct intel_state_flags *a,
			      const struct intel_state_flags *b )
{
   a->mesa |= b->mesa;
   a->intel |= b->intel;
   a->extra |= b->extra;
}


static void xor_states( struct intel_state_flags *result,
			     const struct intel_state_flags *a,
			      const struct intel_state_flags *b )
{
   result->mesa = a->mesa ^ b->mesa;
   result->intel = a->intel ^ b->intel;
   result->extra = a->extra ^ b->extra;
}


/***********************************************************************
 * Emit all state:
 */
void intel_update_software_state( struct intel_context *intel )
{
   struct intel_state_flags *state = &intel->state.dirty;
   GLuint i;

   assert(intel->ctx.NewState == 0);

   if (state->intel == 0) {
      assert(state->mesa == 0);
      assert(state->extra == 0);
      return;
   }

   if (!intel->metaops.active) {
      intel->state.DrawBuffer = intel->ctx.DrawBuffer;
      intel->state.ReadBuffer = intel->ctx.ReadBuffer;
      intel->state.RenderMode = intel->ctx.RenderMode;
      intel->state._ColorDrawBufferMask0 = intel->ctx.DrawBuffer->_ColorDrawBufferMask[0];
   }

   if (!intel->vtbl.check_indirect_space( intel ))
      intel_batchbuffer_flush( intel->batch, GL_FALSE );

   if (INTEL_DEBUG) {
      /* Debug version which enforces various sanity checks on the
       * state flags which are generated and checked to help ensure
       * state atoms are ordered correctly in the list.
       */
      struct intel_state_flags examined, prev;      
      _mesa_memset(&examined, 0, sizeof(examined));
      prev = *state;

      for (i = 0; i < intel->driver_state.nr_atoms; i++) {	 
	 const struct intel_tracked_state *atom = intel->driver_state.atoms[i];
	 struct intel_state_flags generated;

	 assert(atom->dirty.mesa ||
		atom->dirty.intel ||
		atom->dirty.extra);
	 assert(atom->update);

	 if (check_state(state, &atom->dirty)) {
	    intel->driver_state.atoms[i]->update( intel );
	 }

	 accumulate_state(&examined, &atom->dirty);

	 /* generated = (prev ^ state)
	  * if (examined & generated)
	  *     fail;
	  */
	 xor_states(&generated, &prev, state);
	 assert(!check_state(&examined, &generated));
	 prev = *state;
      }
   }
   else {
      const GLuint nr = intel->driver_state.nr_atoms;

      for (i = 0; i < nr; i++) {	 
	 if (check_state(state, &intel->driver_state.atoms[i]->dirty))
	    intel->driver_state.atoms[i]->update( intel );
      }
   }

   memset(state, 0, sizeof(*state));
}


void intel_emit_hardware_state( struct intel_context *intel,
				GLuint dwords )
{
   GLuint i;

   for (i = 0; i < 2; i++)
   {
      intel_update_software_state( intel );
      
      if (intel_batchbuffer_space( intel->batch, SEGMENT_IMMEDIATE ) <
	  intel->vtbl.get_hardware_state_size( intel ) +  dwords * sizeof(GLuint))
      {
	 assert(i == 0);
	 intel_batchbuffer_flush( intel->batch, GL_FALSE );
      }
      else 
      {
	 break;
      }
   } 
      
   intel->vtbl.emit_hardware_state( intel );
}




void intel_state_init( struct intel_context *intel )
{
   GLcontext *ctx = &intel->ctx;

   intel->state.Color = &ctx->Color;
   intel->state.Depth = &ctx->Depth;
   intel->state.Fog = &ctx->Fog;
   intel->state.Hint = &ctx->Hint;
   intel->state.Light = &ctx->Light;
   intel->state.Line = &ctx->Line;
   intel->state.Point = &ctx->Point;
   intel->state.Polygon = &ctx->Polygon;
   intel->state.Scissor = &ctx->Scissor;
   intel->state.Stencil = &ctx->Stencil;
   intel->state.Texture = &ctx->Texture;
   intel->state.Transform = &ctx->Transform;
   intel->state.Viewport = &ctx->Viewport;
   intel->state.VertexProgram = &ctx->VertexProgram;
   intel->state.FragmentProgram = &ctx->FragmentProgram;
   intel->state.PolygonStipple = &ctx->PolygonStipple[0];
}
