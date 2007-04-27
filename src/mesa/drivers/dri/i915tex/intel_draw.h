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

#ifndef INTEL_DRAW_H
#define INTEL_DRAW_H

#include "mtypes.h"
#include "vf/vf.h"


struct intel_draw_vb_state {
   GLuint clipped_prims:1;
   GLuint pad:15;
   GLuint active_prims:16;
};

struct intel_render {
   /* Initialize state for the frame.  EG. emit dma to wait for
    * pending flips.
    */
   void (*start_render)( struct intel_render *,
			 GLboolean start_of_frame );


   /* Request a destination for incoming vertices in the format above.
    * Hardware renderers will use ttm memory, others will just malloc
    * something.  Vertices will be built by some external facility
    * (draw itself, or the prim pipe).
    */
   void *(*allocate_vertices)( struct intel_render *,
			       GLuint vertex_size,
			       GLuint nr_vertices );

   /* Notify the renderer of the current primitive when it changes:
    */
   void (*set_prim)( struct intel_render *, GLuint prim );

   /* DrawArrays:
    */
   void (*draw_prim)( struct intel_render *,
		      GLuint start,
		      GLuint nr );

   /* DrawElements:
    */
   void (*draw_indexed_prim)( struct intel_render *,
			      const GLuint *indices,
			      GLuint nr );


   /* Hardware drivers will flush/unmap the ttm:
    */
   void (*release_vertices)( struct intel_render *,
			     void *vertices );


   /* Execute glFlush(), flag to state whether this is the end of the
    * frame or not, to help choose renderers.
    */
   void (*flush)( struct intel_render *, 
		  GLboolean finished_frame );

   void (*destroy)( struct intel_render * );
};


struct intel_draw_callbacks {
   /* This is opaque to the draw code:
    */
   void *driver;

   /* Called when the primitives or clipping mode of the vb changes.
    * Driver may want to set a new hardware renderer.
    */
   void (*set_vb_state)( void *driver, 
			 const struct intel_draw_vb_state *vb_state  );


   /* Ask driver to validate state at the head of a vb.  May result in
    * calls back into draw to set hw renderer, viewport, etc.
    */
   void (*validate_state)( void *driver  );
};


#define WINDING_NONE 0
#define WINDING_CW   1
#define WINDING_CCW  2
#define WINDING_BOTH (WINDING_CW | WINDING_CCW)

#define FILL_POINT 1
#define FILL_LINE  2
#define FILL_TRI   3

/* A struct containing all the GL state the drawing engine cares
 * about.  The driver state tracker does the job of monitoring gl or
 * metaops state and notifying the drawing engine whenever necessary
 * to keep it uptodate.
 */
struct intel_draw_state {
   /* GL state
    */
   GLuint flatshade:1;
   GLuint light_twoside:1;

   GLuint cull_mode:2;

   GLuint fill_cw:2;
   GLuint fill_ccw:2;

   GLuint offset_cw:1;
   GLuint offset_ccw:1;

   GLfloat offset_units;
   GLfloat offset_scale;
};


struct intel_draw *intel_draw_create( const struct intel_draw_callbacks *callbacks );
				      
void intel_draw_destroy( struct intel_draw * );

void intel_draw_flush( struct intel_draw * );

void intel_draw_finish_frame( struct intel_draw * );

void intel_draw_set_viewport( struct intel_draw *draw,
			      const GLfloat *scale,
			      const GLfloat *trans );

void intel_draw_set_state( struct intel_draw *draw,
			   const struct intel_draw_state *state );

void intel_draw_set_hw_vertex_format( struct intel_draw *draw,
				      const struct vf_attr_map *attr,
				      GLuint count,
				      GLuint vertex_size );


void intel_draw_set_prim_vertex_format( struct intel_draw *draw,
					const struct vf_attr_map *attr,
					GLuint count,
					GLuint vertex_size );

void intel_draw_set_render( struct intel_draw *draw,
			    struct intel_render *render );

void intel_draw_set_prim_pipe_active( struct intel_draw *draw,
				      GLboolean active );


struct vertex_fetch *intel_draw_get_hw_vf( struct intel_draw *draw );

struct tnl_pipeline_stage *intel_draw_tnl_stage( struct intel_draw *draw );

/***********************************************************************
 * Private structs and functions:
 */
#ifdef INTEL_DRAW_PRIVATE

struct intel_draw {     
   struct intel_draw_callbacks callbacks;

   /* The most recent drawing state as set by the driver:
    */
   struct intel_draw_state state;

   /* Primitive/VB state that we send to the driver (and to the prim
    * pipeline).
    */
   struct intel_draw_vb_state vb_state;
   
   /* The hardware backend (or swrast)
    */
   struct intel_render *hw;
   struct vf_attr_map hw_attrs[VF_ATTRIB_MAX];
   GLuint hw_attr_count;
   GLuint hw_vertex_size;
   struct vertex_fetch *hw_vf;

   /* Helper for quads (when prim pipe not active??)
    */
   struct intel_render *quads;


   /* The software clipper/setup engine.  Feeds primitives into the
    * above as necessary:
    */
   struct intel_render *prim;	 
   struct vf_attr_map prim_attrs[VF_ATTRIB_MAX];
   GLuint prim_attr_count;
   GLuint prim_vertex_size;
   GLboolean prim_pipe_active;
   struct vertex_fetch *prim_vf;


   struct {
      /* The active renderer - either quads or prim, depending on gl
       * state and clipped prims.
       */
      struct intel_render *render;
      GLenum render_prim;

      /* The active vf and the attributes installed in vf:
       */
      struct vertex_fetch *vf;
      struct vf_attr_map *attrs;
      GLuint attr_count;
      GLuint vertex_size;

      /* Destination for vertices, allocated by render:
       */
      GLubyte *verts;
   } vb;

   /* State
    */
   GLboolean in_frame;
   GLboolean in_vb;
   GLboolean revalidate;

   /* Helper for tnl:
    */
   GLvector4f header;   
};


#endif
#endif
