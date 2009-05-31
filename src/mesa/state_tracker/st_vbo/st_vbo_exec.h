/**************************************************************************

Copyright 2002 Tungsten Graphics Inc., Cedar Park, Texas.

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
TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

#ifndef __ST_VBO_EXEC_H__
#define __ST_VBO_EXEC_H__

#include "main/mtypes.h"
#include "st_vbo.h"
#include "st_vbo_attrib.h"


#define ST_VBO_MAX_PRIM 64

/* Wierd implementation stuff:
 */
#define ST_VBO_VERT_BUFFER_SIZE (1024*64)	/* bytes */
#define ST_VBO_MAX_ATTR_CODEGEN 16
#define ERROR_ATTRIB 16




struct st_vbo_exec_eval1_map {
   struct gl_1d_map *map;
   GLuint sz;
};

struct st_vbo_exec_eval2_map {
   struct gl_2d_map *map;
   GLuint sz;
};


typedef void (*st_vbo_attrfv_func)( const GLfloat * );

struct st_vbo_context;

struct st_vbo_exec_context
{
   struct st_vbo_context *st_vbo;
//   GLcontext *ctx;
   GLvertexformat vtxfmt;


   struct {
      struct gl_buffer_object *bufferobj;
      GLfloat *buffer_map;
      GLfloat *buffer_ptr;              /* cursor, points into buffer */

      GLuint vert_count;
      GLuint max_vert;
      GLuint vertex_size;       /* in dwords */

      /* This tracks the portion of bufferobj which has been used in
       * previous draw calls and submitted to the driver.  It does
       * *not* track vertex allocations within a primitive.
       */
      GLuint   buffer_used;             /* in bytes */


      /* A list of primitives against the current vertex buffer which
       * are waiting to be submitted to hardware.  The vertex buffer
       * will be unmapped prior to submitting.
       */
      struct st_mesa_prim prim[ST_VBO_MAX_PRIM];
      GLuint prim_count;
      GLenum mode;

      /* Current vertex.  API functions directly update this vertex
       * only.  This data will be copied to one of the vertex slots
       * below, and from there to the vertex buffer.
       */
      GLfloat vertex[ST_VBO_ATTRIB_MAX * 4];

      /* Set of four or fewer emitted vertices for current primitive.
       * Each primitive defines a set of callbacks to implement a
       * state machine which ensures the necessary vertices are held
       * in these slots to allow the current primitive to be wrapped
       * and restarted on full-buffer and other events.
       */
      struct {
         GLfloat vertex[ST_VBO_ATTRIB_MAX * 4];
         void (*vertex_func)( struct st_vbo_exec_context * );
         void (*end_func)( struct st_vbo_exec_context * );
      } slot[4];

      /* Current slot - this is the slot which will be updated on the
       * next call to glVertex().
       */
      unsigned slotnr;

      GLubyte attrsz[ST_VBO_ATTRIB_MAX];
      GLubyte active_sz[ST_VBO_ATTRIB_MAX];
      GLfloat *attrptr[ST_VBO_ATTRIB_MAX];



      /* The data from attr[] expressed as gl_client_array structs:
       */
      struct gl_client_array arrays[ST_VBO_ATTRIB_MAX];

      /* According to program mode, the arrays above, plus current
       * values are squashed down to the actual vertex program inputs
       * below:
       */
      GLuint program_mode;
      GLuint enabled_flags;
      const struct gl_client_array *inputs[VERT_ATTRIB_MAX];
   } vtx;


   struct {
      GLboolean recalculate_maps;
      struct st_vbo_exec_eval1_map map1[VERT_ATTRIB_MAX];
      struct st_vbo_exec_eval2_map map2[VERT_ATTRIB_MAX];
   } eval;

   struct {
      GLuint program_mode;
      GLuint enabled_flags;
      GLuint array_obj;

      /* These just mirror the current arrayobj (todo: make arrayobj
       * look like this and remove the mirror):
       */
      const struct gl_client_array *legacy_array[16];
      const struct gl_client_array *generic_array[16];

      /* Arrays and current values manipulated according to program
       * mode, etc.  These are the attributes as seen by vertex
       * programs:
       */
      const struct gl_client_array *inputs[VERT_ATTRIB_MAX];
   } array;
};



/* External API:
 */
void st_vbo_exec_init( GLcontext *ctx );
void st_vbo_exec_destroy( GLcontext *ctx );
void st_vbo_exec_invalidate_state( GLcontext *ctx, GLuint new_state );
void st_vbo_exec_FlushVertices_internal( GLcontext *ctx, GLboolean unmap );

void st_vbo_exec_BeginVertices( GLcontext *ctx );
void st_vbo_exec_FlushVertices( GLcontext *ctx, GLuint flags );


/* Internal functions:
 */

typedef void (*st_vbo_exec_callback)( struct st_vbo_exec_context * );

st_vbo_exec_callback st_vbo_vertex_funcs[GL_POLYGON+1][4];



void st_vbo_exec_array_init( struct st_vbo_exec_context *exec );
void st_vbo_exec_array_destroy( struct st_vbo_exec_context *exec );


void st_vbo_exec_vtx_init( struct st_vbo_exec_context *exec );
void st_vbo_exec_vtx_destroy( struct st_vbo_exec_context *exec );
void st_vbo_exec_vtx_flush( struct st_vbo_exec_context *exec, GLboolean unmap );
void st_vbo_exec_vtx_map( struct st_vbo_exec_context *exec );

void st_vbo_exec_vtx_bind_arrays( GLcontext *ctx );

void st_vbo_exec_vtx_choke_prim( struct st_vbo_exec_context *exec );

void st_vbo_exec_fixup_vertex( struct st_vbo_exec_context *exec,
                               GLuint attr,
                               GLuint sz );


void st_vbo_exec_eval_update( struct st_vbo_exec_context *exec );

void st_vbo_exec_do_EvalCoord2f( struct st_vbo_exec_context *exec,
				     GLfloat u, GLfloat v );

void st_vbo_exec_do_EvalCoord1f( struct st_vbo_exec_context *exec,
				     GLfloat u);

extern GLboolean
st_vbo_validate_shaders(GLcontext *ctx);

#endif
