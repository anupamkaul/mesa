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

struct intel_vb *intel_vb_init( struct intel_context *intel );
void intel_vb_destroy( struct intel_vb *vb );

/* Need to talk about allocations in terms of vertices of a particular
 * size so that we can manage the index-offset state correctly and
 * also know when to rebase the vbo even if we haven't run out of
 * space (ie on a vertex size change).
 */
void intel_vb_set_vertex_size( struct intel_vb *vb,
			       GLuint vertex_size );

/* Returns the vertex offset from the programmed vbo starting
 * position.  Returns the pointer to the allocated region in the
 * ptr_return argument.
 */
void *intel_vb_alloc_vertices( struct intel_vb *vb,
			       GLuint count,
			       GLuint *offset_return );

void intel_vb_flush( struct intel_vb *vb );

/***********************************************************************
 * Internal functions
 */

#define MAX_VBO 32

struct intel_vb {
   struct intel_context *intel;

   struct {
      struct intel_buffer_object *vbo[MAX_VBO];
      GLuint idx;

      struct intel_buffer_object *current;

      GLuint current_size;
      GLuint current_used;
      void *current_ptr;

      GLuint wrap_vbo;
   } vbo;

   /* State for hardware vertex emit: 
    */
   struct intel_buffer_object *buffer;
   GLuint offset;
   GLboolean dirty;
      
   GLuint vertex_size_bytes;

   GLuint nr_verts;
   GLuint space;
};



void intel_vb_unmap_current_vbo( struct intel_vb *vb );




#endif
