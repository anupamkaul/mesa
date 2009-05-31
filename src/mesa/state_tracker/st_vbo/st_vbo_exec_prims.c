/**************************************************************************

Copyright 2009 VMware, Inc

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
VMWARE, INC AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keithw@vmware.com>
 */

#include "main/glheader.h"
#include "main/bufferobj.h"
#include "main/context.h"
#include "main/macros.h"
#include "main/vtxfmt.h"

#include "st_vbo_exec.h"

static void end_prim( struct st_vbo_exec_context *exec );


/* Prevent extend_prim() from succeeding on the next call.  That will
 * force a wrap_prim() and re-emit of duplicated vertices at some
 * point in the future.
 */
void st_vbo_exec_vtx_choke_prim( struct st_vbo_exec_context *exec )
{
   exec->vtx.max_vert = 0;
}


/* Allocate additional vertex data for a primitive which has already
 * been started.
 */
static INLINE char *extend_prim( struct st_vbo_exec_context *exec,
                                 unsigned verts )
{
   if (exec->vtx.vert_count + verts > exec->vtx.max_vert)
      return NULL;

   {
      unsigned offset = exec->vtx.vert_count * exec->vtx.vertex_size;

      exec->vtx.vert_count += verts;

      return (char *)(exec->vtx.buffer_map + offset);
   }
}


/* Start a primitive and allocate the initial vertices for it.  Note
 * that this should be called with at least the minimum number of
 * vertices required to properly start a primitive, eg. for tristrips,
 * verts should be 3 or more.
 */
static char *new_prim( struct st_vbo_exec_context *exec,
                       GLenum mode,
                       unsigned verts )
{
   unsigned i;

   if (exec->vtx.prim_count == ST_VBO_MAX_PRIM ||
       exec->vtx.max_vert == 0)
      st_vbo_exec_vtx_flush( exec, GL_FALSE );

   i = exec->vtx.prim_count;
   exec->vtx.prim[i].mode = mode;
   exec->vtx.prim[i].begin = 1;
   exec->vtx.prim[i].end = 0;
   exec->vtx.prim[i].indexed = 0;
   exec->vtx.prim[i].weak = 0;
   exec->vtx.prim[i].pad = 0;
   exec->vtx.prim[i].start = exec->vtx.vert_count;
   exec->vtx.prim[i].count = 0;

   /* Install default end_prim function:
    */
   exec->vtx.slot[0].end_func = end_prim;
   exec->vtx.slot[1].end_func = end_prim;
   exec->vtx.slot[2].end_func = end_prim;
   exec->vtx.slot[3].end_func = end_prim;

   return extend_prim( exec, verts );
}

/* Placeholder that catches GL begin/ends that don't actually spawn a
 * primitive, such as two-vertex triangles.
 */
static void end_prim_noop( struct st_vbo_exec_context *exec )
{
}


/* Default handler for ends for which all vertices have been emitted.
 * Only lineloop needs to override this.
 */
static void end_prim( struct st_vbo_exec_context *exec )
{
   int i = exec->vtx.prim_count;

   exec->vtx.prim[i].end = 1;
   exec->vtx.prim[i].count = (exec->vtx.vert_count -
                              exec->vtx.prim[i].start);
   exec->vtx.prim_count++;

   /* Install dummy end_prim function:
    */
   exec->vtx.slot[0].end_func = end_prim_noop;
   exec->vtx.slot[1].end_func = end_prim_noop;
   exec->vtx.slot[2].end_func = end_prim_noop;
   exec->vtx.slot[3].end_func = end_prim_noop;
}



/* Finish off the current primitive, flush primitives to backend,
 * allocate a new vbo and start a new primitive.  The caller will
 * ensure that wrapped vertices are re-emitted.
 */
static char *wrap_prim( struct st_vbo_exec_context *exec,
                        GLenum prim,
                        unsigned verts )
{
   int i = exec->vtx.prim_count;

   assert(exec->vtx.prim[i].mode == prim);

   exec->vtx.prim[i].end = 0;
   exec->vtx.prim[i].count = (exec->vtx.vert_count -
                              exec->vtx.prim[i].start);
   exec->vtx.prim_count++;


   /* Do the following:
    *
    *   - unmap_vbo();
    *   - fire primitives to backend()
    *   - get_new_vbo();
    *   - map_vbo();
    */
   st_vbo_exec_vtx_flush( exec, GL_FALSE );

   return new_prim( exec, prim, verts );
}

static INLINE void emit_vertex( struct st_vbo_exec_context *exec,
                                char **destptr,
                                unsigned slot )
{
   char *dest = *destptr;

   {
      unsigned i;

      _mesa_printf("%s slot %d dest: %p\n",
                   __FUNCTION__,
                   slot,
                   dest);

      for (i = 0; i < exec->vtx.vertex_size; i++)
         _mesa_printf("   %d: %f\n", i, exec->vtx.slot[slot].vertex[i]);
   }

   memcpy( dest,
           exec->vtx.slot[slot].vertex,
           exec->vtx.vertex_size * sizeof(GLfloat));

   *destptr = dest + exec->vtx.vertex_size * sizeof(GLfloat);
}



/* POINTS
 */
static void emit_point_subsequent_slot_zero( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POINTS, 1 );
   }

   emit_vertex( exec, &dest, 0 );
   exec->vtx.slotnr = 0;
}

static void emit_point_first_slot_zero( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_POINTS, 1 );
   emit_vertex( exec, &dest, 0 );
   exec->vtx.slotnr = 0;
   exec->vtx.slot[0].vertex_func = emit_point_subsequent_slot_zero;
}


/* LINES
 */
static void emit_line_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINES, 2 );
   }

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 0;
}

static void emit_line_first_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_LINES, 2 );
   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 0;
   exec->vtx.slot[1].vertex_func = emit_line_subsequent_slot_one;
}

/* LINE_STRIP
 */
static void emit_linestrip_subsequent_slot_zero( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 0 );
   exec->vtx.slotnr = 1;
}


static void emit_linestrip_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, &dest, 0 );
   }

   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 0;
}


static void emit_linestrip_first_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_LINE_STRIP, 2 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 1;
   exec->vtx.slot[0].vertex_func = emit_linestrip_subsequent_slot_zero;
   exec->vtx.slot[1].vertex_func = emit_linestrip_subsequent_slot_one;
}


/* LINE_LOOP
 */
static void emit_lineloop_end_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, &dest, 2 );
   }

   emit_vertex( exec, &dest, 0 );
}


static void emit_lineloop_end_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 0 );
}


static void emit_lineloop_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      exec->vtx.slot[2].end_func = emit_lineloop_end_slot_two;
      exec->vtx.slot[1].end_func = emit_lineloop_end_slot_one;
      emit_vertex( exec, &dest, 2 );
   }

   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
}


static void emit_lineloop_subsequent_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_LINE_STRIP, 2 );
      exec->vtx.slot[2].end_func = emit_lineloop_end_slot_two;
      exec->vtx.slot[1].end_func = emit_lineloop_end_slot_one;
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 1;
}


static void emit_lineloop_first_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_LINE_STRIP, 2 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
   exec->vtx.slot[1].vertex_func = emit_lineloop_subsequent_slot_one;
   exec->vtx.slot[2].vertex_func = emit_lineloop_subsequent_slot_two;
   exec->vtx.slot[2].end_func = emit_lineloop_end_slot_two;
   exec->vtx.slot[1].end_func = emit_lineloop_end_slot_one;
}




/* TRIANGLES
 */
static void emit_triangle_subsequent_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 3 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLES, 3 );
   }

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 0;
}

static void emit_triangle_first_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLES, 3 );
   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 0;
   exec->vtx.slot[2].vertex_func = emit_triangle_subsequent_slot_two;
}

/* TRIANGLE_STRIP
 */

static void emit_tristrip_subsequent_slot_zero( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, &dest, 2 );
      emit_vertex( exec, &dest, 3 );
   }

   emit_vertex( exec, &dest, 0 );
   exec->vtx.slotnr = 1;
}

static void emit_tristrip_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 3 );
   }

   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
}

static void emit_tristrip_subsequent_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 3;
}


static void emit_tristrip_subsequent_slot_three( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_STRIP, 3 );
      emit_vertex( exec, &dest, 2 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 3 );
   exec->vtx.slotnr = 0;
}


static void emit_tristrip_first_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLE_STRIP, 3 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 3;
   exec->vtx.slot[0].vertex_func = emit_tristrip_subsequent_slot_zero;
   exec->vtx.slot[1].vertex_func = emit_tristrip_subsequent_slot_one;
   exec->vtx.slot[2].vertex_func = emit_tristrip_subsequent_slot_two;
   exec->vtx.slot[3].vertex_func = emit_tristrip_subsequent_slot_three;
}



/* TRIANGLE_FAN
 */
static void emit_trifan_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_FAN, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 2 );
   }

   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
}

static void emit_trifan_subsequent_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_TRIANGLE_FAN, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 1;
}


static void emit_trifan_first_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_TRIANGLE_STRIP, 3 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 1;
   exec->vtx.slot[1].vertex_func = emit_trifan_subsequent_slot_one;
   exec->vtx.slot[2].vertex_func = emit_trifan_subsequent_slot_two;
}



/* QUADS
 */
static void emit_quad_subsequent_slot_three( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 4 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
   }

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   emit_vertex( exec, &dest, 4 );
   exec->vtx.slotnr = 0;
}


static void emit_quad_first_slot_three( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_QUAD_STRIP, 4 );
   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   emit_vertex( exec, &dest, 4 );
   exec->vtx.slotnr = 0;
   exec->vtx.slot[3].vertex_func = emit_quad_subsequent_slot_three;
}

/* QUADSTRIP
 */

static void emit_quadstrip_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
      emit_vertex( exec, &dest, 2 );
      emit_vertex( exec, &dest, 3 );
   }

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
}

static void emit_quadstrip_subsequent_slot_three( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 2 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_QUAD_STRIP, 4 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 2 );
   emit_vertex( exec, &dest, 3 );
   exec->vtx.slotnr = 0;
}


static void emit_quadstrip_first_slot_three( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_QUAD_STRIP, 4 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   emit_vertex( exec, &dest, 3 );
   exec->vtx.slotnr = 0;
   exec->vtx.slot[1].vertex_func = emit_quadstrip_subsequent_slot_one;
   exec->vtx.slot[3].vertex_func = emit_quadstrip_subsequent_slot_three;
}

/* POLYGON
 */
static void emit_polygon_subsequent_slot_one( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POLYGON, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 2 );
   }

   emit_vertex( exec, &dest, 1 );
   exec->vtx.slotnr = 2;
}

static void emit_polygon_subsequent_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = extend_prim( exec, 1 );

   if (dest == 0) {
      dest = wrap_prim( exec, GL_POLYGON, 3 );
      emit_vertex( exec, &dest, 0 );
      emit_vertex( exec, &dest, 1 );
   }

   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 1;
}


static void emit_polygon_first_slot_two( struct st_vbo_exec_context *exec )
{
   char *dest = new_prim( exec, GL_POLYGON, 3 );

   emit_vertex( exec, &dest, 0 );
   emit_vertex( exec, &dest, 1 );
   emit_vertex( exec, &dest, 2 );
   exec->vtx.slotnr = 1;
   exec->vtx.slot[1].vertex_func = emit_polygon_subsequent_slot_one;
   exec->vtx.slot[2].vertex_func = emit_polygon_subsequent_slot_two;
}

/* Noop
 */
static void emit_noop( struct st_vbo_exec_context *exec )
{
   exec->vtx.slotnr++;
}

st_vbo_exec_callback st_vbo_vertex_funcs[GL_POLYGON+1][4] =
{
   { emit_point_first_slot_zero,
     NULL,
     NULL,
     NULL },

   { emit_noop,
     emit_line_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_lineloop_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_linestrip_first_slot_one,
     NULL,
     NULL },

   { emit_noop,
     emit_noop,
     emit_triangle_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_tristrip_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_trifan_first_slot_two,
     NULL },

   { emit_noop,
     emit_noop,
     emit_noop,
     emit_quad_first_slot_three },

   { emit_noop,
     emit_noop,
     emit_noop,
     emit_quadstrip_first_slot_three },

   { emit_noop,
     emit_noop,
     emit_polygon_first_slot_two,
     NULL }
};
