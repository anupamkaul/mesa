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

/* Author:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "imports.h"
#include "macros.h"

#include "tnl/t_context.h"
#include "vf/vf.h"

#define CLIP_PRIVATE
#include "clip/clip_context.h"

#include "clip/clip_pipe.h"
#include "clip/clip_noop.h"



/* Called when driver state tracker notices changes to the viewport
 * matrix:
 */
void clip_set_viewport( struct clip_context *clip,
			      const GLfloat *scale,
			      const GLfloat *trans )
{
   assert(!clip->in_vb);
   vf_set_vp_scale_translate( clip->hw_vf, scale, trans );
   vf_set_vp_scale_translate( clip->prim_vf, scale, trans );
}


void clip_set_state( struct clip_context *clip,
			   const struct clip_state *state )
{
   memcpy( &clip->state, state, sizeof(*state) );

   /* Need to validate state.
    */
   clip->revalidate = 1;
}


/* Called when driver state tracker notices changes to the hardware
 * vertex format:
 */
void clip_set_hw_vertex_format( struct clip_context *clip,
				      const struct vf_attr_map *attr,
				      GLuint count,
				      GLuint vertex_size )
{
   /* This is not allowed during the processing of a vertex buffer, ie
    * vertex format must be constant throughout a whole VB and cannot
    * change per-primitive.  This implies that the swrast fallback
    * code must be able to translate the hardware vertices into swrast
    * vertices on the fly.
    */
   assert(!clip->in_vb);

   
   memcpy(clip->hw_attrs, attr, count * sizeof(*attr));
   clip->hw_attr_count = count;
   clip->hw_vertex_size = vertex_size;

   vf_set_vertex_attributes( clip->hw_vf,
			     clip->hw_attrs,
			     clip->hw_attr_count,
			     clip->hw_vertex_size );


   clip->revalidate = 1;
}


void clip_set_prim_vertex_format( struct clip_context *clip,
					const struct vf_attr_map *attr,
					GLuint count,
					GLuint vertex_size )
{
   assert(!clip->in_vb);
   
   memcpy(clip->prim_attrs, attr, count * sizeof(*attr));
   clip->prim_attr_count = count;
   clip->prim_vertex_size = vertex_size;

   vf_set_vertex_attributes( clip->prim_vf,
			     clip->prim_attrs,
			     clip->prim_attr_count,
			     clip->prim_vertex_size );


   clip->revalidate = 1;
}


/* Called when the driver state tracker notices a fallback condition
 * or other reason to change the renderer.  Note that per-primitive
 * fallbacks cannot be handled this way - use a multiplexing render
 * module instead.
 */
void clip_set_render( struct clip_context *clip,
			    struct clip_render *hw )
{
   /* This is not allowed during the processing of a vertex buffer.
    */
   assert( !clip->in_vb );

   /* Install the new one.
    */
   clip->hw = hw;
   clip->revalidate = 1;
}

void clip_set_userclip( struct clip_context *clip,
			      GLfloat (* const ucp)[4],
			      GLuint nr )
{
   memcpy(&clip->plane[6], ucp, nr * sizeof(ucp[0]));
   clip->nr_planes = 6 + nr;
}


static void clip_validate_state( struct clip_context *clip )
{
   /* Choose between simple and complex (quads vs. prim) pipelines.
    */
   clip->prim_pipe_active = clip_pipe_validate_state( clip->prim );

   if (clip->prim_pipe_active) {
      clip_pipe_set_hw_render( clip->prim, clip->hw );

      clip->vb.render = clip->prim;
      clip->vb.attrs = clip->prim_attrs;
      clip->vb.attr_count = clip->prim_attr_count;
      clip->vb.vertex_size = clip->prim_vertex_size;
      clip->vb.vf = clip->prim_vf;
   }
   else {
      clip_noop_set_hw_render( clip->noop, clip->hw );

      clip->vb.render = clip->noop;
      clip->vb.attrs = clip->hw_attrs;
      clip->vb.attr_count = clip->hw_attr_count;
      clip->vb.vertex_size = clip->hw_vertex_size;
      clip->vb.vf = clip->hw_vf;
   }

   clip->vb.render->set_prim( clip->vb.render, 
			      clip->vb.render_prim );

   clip->revalidate = 0;
}


struct vertex_fetch *clip_get_hw_vf( struct clip_context *clip )
{
   return clip->hw_vf;
}


struct clip_context *clip_create( const struct clip_callbacks *callbacks )
{
   struct clip_context *clip = CALLOC_STRUCT(clip_context);

   memcpy( &clip->callbacks, callbacks, sizeof(*callbacks) );

   clip->prim = clip_create_prim_render( clip );
   clip->noop = clip_create_noop_render( clip );

   clip->hw_vf = vf_create( GL_TRUE );
   clip->prim_vf = vf_create( GL_TRUE );

   ASSIGN_4V( clip->plane[0], -1,  0,  0, 1 );
   ASSIGN_4V( clip->plane[1],  1,  0,  0, 1 );
   ASSIGN_4V( clip->plane[2],  0, -1,  0, 1 );
   ASSIGN_4V( clip->plane[3],  0,  1,  0, 1 );
   ASSIGN_4V( clip->plane[4],  0,  0,  1, 1 ); /* yes these are correct */
   ASSIGN_4V( clip->plane[5],  0,  0, -1, 1 ); /* mesa's a bit wonky */
   clip->nr_planes = 6;

   return clip;
}


void clip_destroy( struct clip_context *clip )
{
   if (clip->header.storage)
      ALIGN_FREE( clip->header.storage );

   clip->prim->destroy( clip->prim );
   clip->noop->destroy( clip->noop );

   vf_destroy( clip->hw_vf );
   vf_destroy( clip->prim_vf );

   FREE( clip );
}







static const GLenum reduced_prim[GL_POLYGON+1] = {
   GL_POINTS,
   GL_LINES,
   GL_LINES,
   GL_LINES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES,
   GL_TRIANGLES
};


static void 
build_vertex_headers( struct clip_context *clip,
		      struct vertex_buffer *VB )
{
   if (clip->header.storage == NULL) {
      clip->header.stride = sizeof(GLfloat);
      clip->header.size = 1;
      clip->header.storage = ALIGN_MALLOC( VB->Size * sizeof(GLfloat), 32 );
      clip->header.data = clip->header.storage;
      clip->header.count = 0;
      clip->header.flags = VEC_SIZE_1 | VEC_MALLOC;
   }

   /* Build vertex header attribute.
    * 
    */

   {
      GLuint i;
      struct vertex_header *header = (struct vertex_header *)clip->header.storage;

      /* yes its a hack
       */
      assert(sizeof(*header) == sizeof(GLfloat));

      clip->header.count = VB->Count;

      if (clip->state.fill_cw != FILL_TRI ||
	  clip->state.fill_ccw != FILL_TRI) {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = VB->ClipMask[i];
	    header[i].edgeflag = VB->EdgeFlag[i]; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
      else if (VB->ClipOrMask) {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = VB->ClipMask[i];
	    header[i].edgeflag = 0; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
      else {
	 for (i = 0; i < VB->Count; i++) {
	    header[i].clipmask = 0;
	    header[i].edgeflag = 0; 
	    header[i].pad = 0;
	    header[i].index = 0xffff;
	 }
      }
   }

   VB->AttribPtr[VF_ATTRIB_VERTEX_HEADER] = &clip->header;
}

static void 
update_vb_state( struct clip_context *clip,
		 struct vertex_buffer *VB )
{
   struct clip_vb_state vb_state;
   GLuint i;

   vb_state.clipped_prims = (VB->ClipOrMask != 0);
   vb_state.pad = 0;
   vb_state.active_prims = 0;

   /* Build a bitmask of the reduced primitives in this vb:
    */
   for (i = 0; i < VB->PrimitiveCount; i++)
      vb_state.active_prims |= 1 << reduced_prim[VB->Primitive[i].mode];

   if (memcmp(&vb_state, &clip->vb_state, sizeof(vb_state)) != 0) {
      
      memcpy(&clip->vb_state, &vb_state, sizeof(vb_state));


      /* Tell driver about active primitives, let it update render before
       * starting the vb.
       */
      clip->callbacks.set_vb_state( clip->callbacks.driver,
				    &vb_state );

      clip_pipe_set_vb_state( clip->prim, &vb_state );
   }
}




GLuint clip_prim_info(GLenum mode, GLuint *first, GLuint *incr)
{
   switch (mode) {
   case GL_POINTS:
      *first = 1;
      *incr = 1;
      return 0;
   case GL_LINES:
      *first = 2;
      *incr = 2;
      return 0;
   case GL_LINE_STRIP:
      *first = 2;
      *incr = 1;
      return 0;
   case GL_LINE_LOOP:
      *first = 2;
      *incr = 1;
      return 1;
   case GL_TRIANGLES:
      *first = 3;
      *incr = 3;
      return 0;
   case GL_TRIANGLE_STRIP:
      *first = 3;
      *incr = 1;
      return 0;
   case GL_TRIANGLE_FAN:
   case GL_POLYGON:
      *first = 3;
      *incr = 1;
      return 1;
   case GL_QUADS:
      *first = 4;
      *incr = 4;
      return 0;
   case GL_QUAD_STRIP:
      *first = 4;
      *incr = 2;
      return 0;
   default:
      assert(0);
      *first = 1;
      *incr = 1;
      return 0;
   }
}


static GLuint trim( GLuint count, GLuint first, GLuint incr )
{
   if (count < first)
      return 0;
   else
      return count - (count - first) % incr; 
}



void clip_vb(struct clip_context *clip,
	     struct vertex_buffer *VB )
{
   GLuint i;

   update_vb_state( clip, VB );

   clip->callbacks.validate_state( clip->callbacks.driver );
   
   /* Validate our render pipeline: 
    */
   if (clip->revalidate)
      clip_validate_state( clip );


   /* Maybe build vertex headers: 
    */
   if (clip->prim_pipe_active) {
      VB->AttribPtr[VF_ATTRIB_BFC0] = VB->ColorPtr[1];
      VB->AttribPtr[VF_ATTRIB_BFC1] = VB->SecondaryColorPtr[1];
      VB->AttribPtr[VF_ATTRIB_CLIP_POS] = VB->ClipPtr;

      build_vertex_headers( clip, VB );
   }

   clip->in_vb = 1;

   /* Allocate the vertices:
    */
   clip->vb.verts = clip->vb.render->allocate_vertices( clip->vb.render,
							clip->vb.vertex_size,
							VB->Count );

   /* Bind the vb outputs:
    */
   vf_set_sources( clip->vb.vf, VB->AttribPtr, 0 );

   /* Build the hardware or prim-pipe vertices: 
    */
   vf_emit_vertices( clip->vb.vf,
		     VB->Count,
		     clip->vb.verts );


   for (i = 0; i < VB->PrimitiveCount; i++) {

      GLenum mode = VB->Primitive[i].mode;
      GLuint start = VB->Primitive[i].start;
      GLuint length, first, incr;

      /* Trim the primitive down to a legal size.  
       */
      clip_prim_info( mode, &first, &incr );
      length = trim( VB->Primitive[i].count, first, incr );

      if (!length)
	 continue;

      if (clip->vb.render_prim != mode) {
	 clip->vb.render_prim = mode;
	 clip->vb.render->set_prim( clip->vb.render, mode );
      }

      if (VB->Elts) {
	 clip->vb.render->draw_indexed_prim( clip->vb.render, 
					     VB->Elts + start,
					     length );
      }
      else {
	 clip->vb.render->draw_prim( clip->vb.render, 
				     start,
				     length );
      }	 
   }

   clip->vb.render->release_vertices( clip->vb.render, clip->vb.verts );
   clip->vb.verts = NULL;
   clip->in_vb = 0;
}


