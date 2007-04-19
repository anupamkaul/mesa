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

#ifndef INTEL_VB_H
#define INTEL_VB_H


struct intel_context;
struct intel_buffer_object;
struct tnl_attr_map;

#include "tnl/t_context.h"


#define MAX_VBO 32		/* XXX: make dynamic */

#define VB_LOCAL_VERTS 0x1
#define VB_HW_VERTS    0x2


struct intel_vb {
   struct intel_context *intel;
   TNLcontext *tnl;

   /* State for hardware vertex emit: 
    */
   struct {
      struct intel_buffer_object *vbo[MAX_VBO];
      GLuint idx;

      struct intel_buffer_object *current;
      GLuint current_size;
      GLuint current_used;
      void *current_ptr;
      GLuint wrap_vbo;
      GLboolean dirty;

      GLuint offset;
   } vbo;
      
   /* The currently built software vertex list:
    */
   struct {
      GLubyte *verts;              /* points to tnl->clipspace.vertex_buf */
      GLboolean dirty;
   } local;

   GLuint vertex_size_bytes;
/*    GLuint vertex_stride_bytes; */
   GLuint nr_verts;
};


static INLINE void *intel_vb_get_vertex( struct intel_vb *vb, GLuint i )
{
   return (void *) vb->local.verts + i * vb->vertex_size_bytes;
}


struct intel_vb *intel_vb_init( struct intel_context *intel );
void intel_vb_destroy( struct intel_vb *vb );

void intel_vb_flush( struct intel_vb *vb );

void intel_vb_set_inputs( struct intel_vb *vb,
			  const struct tnl_attr_map *attrs,
			  GLuint count );

void intel_vb_new_vertices( struct intel_vb *vb );
void intel_vb_release_vertices( struct intel_vb *vb );

GLboolean intel_vb_validate_vertices( struct intel_vb *vb,
				      GLuint flags );

GLuint intel_vb_get_vbo_index_offset( struct intel_vb *vb );


/* Internal functions
 */
GLboolean intel_vb_copy_hw_vertices( struct intel_vb *vb );
GLboolean intel_vb_emit_hw_vertices( struct intel_vb *vb );
void intel_vb_unmap_current_vbo( struct intel_vb *vb );




#endif
