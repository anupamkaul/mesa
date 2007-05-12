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

#ifndef CLIP_CONTEXT_H
#define CLIP_CONTEXT_H

#include "mtypes.h"
#include "vf/vf.h"

struct vertex_buffer;


struct clip_vb_state {
   GLuint clipped_prims:1;
   GLuint pad:15;
   GLuint active_prims:16;
};

struct clip_render {

   struct {
      GLuint max_indices;
   } limits;


   /* Initialize state for the frame.  EG. emit dma to wait for
    * pending flips.
    */
   void (*start_render)( struct clip_render *,
			 GLboolean start_of_frame );


   /* Request a destination for incoming vertices in the format above.
    * Hardware renderers will use ttm memory, others will just malloc
    * something.  Vertices will be built by some external facility
    * (draw itself, or the prim pipe).
    */
   void *(*allocate_vertices)( struct clip_render *,
			       GLuint vertex_size,
			       GLuint nr_vertices );

   /* Notify the renderer of the current primitive when it changes:
    */
   void (*set_prim)( struct clip_render *, GLuint prim );

   /* Driver state passed as a void pointer.  
    */
   void (*emit_state)( struct clip_render *render,
		       const void *driver_state );

   /* DrawArrays:
    */
   void (*draw_prim)( struct clip_render *,
		      GLuint start,
		      GLuint nr );

   /* DrawElements:
    */
   void (*draw_indexed_prim)( struct clip_render *,
			      const GLuint *indices,
			      GLuint nr );


   /* Special primitive: 
    */
   void (*clear_rect)( struct clip_render *,
		       GLuint mask,
		       GLuint x1, GLuint y1, 
		       GLuint x2, GLuint y2 );

   /* Hardware drivers will flush/unmap the ttm:
    */
   void (*release_vertices)( struct clip_render *,
			     void *vertices );


   /* Execute glFlush(), flag to state whether this is the end of the
    * frame or not, to help choose renderers.
    */
   void (*flush)( struct clip_render *, 
		  GLboolean finished_frame );

   void (*destroy)( struct clip_render * );
};


struct clip_callbacks {
   /* This is opaque to the draw code:
    */
   void *driver;

   /* Called when the primitives or clipping mode of the vb changes.
    * Driver may want to set a new hardware renderer.
    */
   void (*set_vb_state)( void *driver, 
			 const struct clip_vb_state *vb_state  );


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
struct clip_state {
   /* GL state
    */
   GLuint flatshade:1;
   GLuint light_twoside:1;

   GLuint front_winding:2;

   GLuint cull_mode:2;

   GLuint fill_cw:2;
   GLuint fill_ccw:2;

   GLuint offset_cw:1;
   GLuint offset_ccw:1;

   GLfloat offset_units;
   GLfloat offset_scale;
};


struct clip_context *clip_create( const struct clip_callbacks *callbacks );
				      
void clip_destroy( struct clip_context * );

void clip_set_viewport( struct clip_context *draw,
			      const GLfloat *scale,
			      const GLfloat *trans );

void clip_set_state( struct clip_context *draw,
			   const struct clip_state *state );


void clip_set_userclip( struct clip_context *draw,
			      GLfloat (* const ucp)[4],
			      GLuint nr );

void clip_set_hw_vertex_format( struct clip_context *draw,
				      const struct vf_attr_map *attr,
				      GLuint count,
				      GLuint vertex_size );


void clip_set_prim_vertex_format( struct clip_context *draw,
					const struct vf_attr_map *attr,
					GLuint count,
					GLuint vertex_size );

void clip_set_render( struct clip_context *draw,
			    struct clip_render *render );

void clip_set_prim_pipe_active( struct clip_context *draw,
				      GLboolean active );


struct vertex_fetch *clip_get_hw_vf( struct clip_context *draw );

void clip_vb(struct clip_context *draw, struct vertex_buffer *VB );

/***********************************************************************
 * Private structs and functions:
 */
#ifdef CLIP_PRIVATE

struct clip_context {     
   struct clip_callbacks callbacks;

   /* The most recent drawing state as set by the driver:
    */
   struct clip_state state;

   /* Primitive/VB state that we send to the driver (and to the prim
    * pipeline).
    */
   struct clip_vb_state vb_state;
   
   GLfloat plane[12][4];
   GLuint nr_planes;
   
   
   /* The hardware backend (or swrast)
    */
   struct clip_render *hw;
   struct vf_attr_map hw_attrs[VF_ATTRIB_MAX];
   GLuint hw_attr_count;
   GLuint hw_vertex_size;
   struct vertex_fetch *hw_vf;

   /* Helper for hardware (when prim pipe not active).
    */
   struct clip_render *noop;


   /* The software clipper/setup engine.  Feeds primitives into the
    * above as necessary:
    */
   struct clip_render *prim;	 
   struct vf_attr_map prim_attrs[VF_ATTRIB_MAX];
   GLuint prim_attr_count;
   GLuint prim_vertex_size;
   GLboolean prim_pipe_active;
   struct vertex_fetch *prim_vf;


   struct {
      /* The active renderer - either noop or pipe, depending on gl
       * state and clipped prims.
       */
      struct clip_render *render;
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
   GLboolean in_vb;
   GLboolean revalidate;

   /* Helper for tnl:
    */
   GLvector4f header;   
};


#endif
#endif
