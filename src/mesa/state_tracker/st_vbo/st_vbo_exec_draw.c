/*
 * Mesa 3-D graphics library
 * Version:  7.2
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "main/glheader.h"
#include "main/bufferobj.h"
#include "main/context.h"
#include "main/enums.h"
#include "main/state.h"
#include "main/macros.h"

#include "st_vbo_context.h"


static void st_vbo_exec_debug_verts( struct st_vbo_exec_context *exec )
{
   GLuint count = exec->vtx.vert_count;
   GLuint i;

   _mesa_printf("%s: %u vertices %d primitives, %d vertsize\n",
		__FUNCTION__,
		count,
		exec->vtx.prim_count,
		exec->vtx.vertex_size);

   for (i = 0 ; i < exec->vtx.prim_count ; i++) {
      struct st_mesa_prim *prim = &exec->vtx.prim[i];
      _mesa_printf("   prim %d: %s%s %d..%d %s %s\n",
		   i,
		   _mesa_lookup_enum_by_nr(prim->mode),
		   prim->weak ? " (weak)" : "",
		   prim->start,
		   prim->start + prim->count,
		   prim->begin ? "BEGIN" : "(wrap)",
		   prim->end ? "END" : "(wrap)");
   }
}





/* TODO: populate these as the vertex is defined:
 */
void st_vbo_exec_vtx_bind_arrays( GLcontext *ctx )
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct st_vbo_exec_context *exec = &st_vbo->exec;
   struct gl_client_array *arrays = exec->vtx.arrays;
   GLuint count = exec->vtx.vert_count;
   GLubyte *data;
   GLsizeiptr offset;
   const GLuint *map;
   GLuint attr;
   GLbitfield varying_inputs = 0x0;

   /* If this is a real buffer obj, we need an offset not a pointer.
    * Otherwise we want the real pointer.
    */
   if (exec->vtx.bufferobj->Name)
      data = NULL;
   else
      data = (GLubyte *)exec->vtx.buffer_map;

   /* Install the default (ie Current) attributes first, then overlay
    * all active ones.
    */
   switch (get_program_mode(ctx)) {
   case VP_NONE:
      for (attr = 0; attr < 16; attr++) {
         exec->vtx.inputs[attr] = &st_vbo->legacy_currval[attr];
      }
      for (attr = 0; attr < MAT_ATTRIB_MAX; attr++) {
         exec->vtx.inputs[attr + 16] = &st_vbo->mat_currval[attr];
      }
      map = st_vbo->map_vp_none;
      break;
   case VP_NV:
   case VP_ARB:
      /* The aliasing of attributes for NV vertex programs has already
       * occurred.  NV vertex programs cannot access material values,
       * nor attributes greater than VERT_ATTRIB_TEX7.
       */
      for (attr = 0; attr < 16; attr++) {
         exec->vtx.inputs[attr] = &st_vbo->legacy_currval[attr];
         exec->vtx.inputs[attr + 16] = &st_vbo->generic_currval[attr];
      }
      map = st_vbo->map_vp_arb;

      /* check if VERT_ATTRIB_POS is not read but VERT_BIT_GENERIC0 is read.
       * In that case we effectively need to route the data from
       * glVertexAttrib(0, val) calls to feed into the GENERIC0 input.
       */
      if ((ctx->VertexProgram._Current->Base.InputsRead & VERT_BIT_POS) == 0 &&
          (ctx->VertexProgram._Current->Base.InputsRead & VERT_BIT_GENERIC0)) {
         exec->vtx.inputs[16] = exec->vtx.inputs[0];
         exec->vtx.attrsz[16] = exec->vtx.attrsz[0];
         exec->vtx.attrsz[0] = 0;
      }
      break;
   default:
      assert(0);
   }

   /* Make all active attributes (including edgeflag) available as
    * arrays of floats.
    */
   for (attr = 0; attr < VERT_ATTRIB_MAX ; attr++) {
      const GLuint src = map[attr];

      if (exec->vtx.attrsz[src]) {
         /* override the default array set above */
         exec->vtx.inputs[attr] = &arrays[attr];


         offset = (GLbyte *) exec->vtx.attrptr[src] - (GLbyte *) exec->vtx.vertex;
         assert(offset >= 0);

         arrays[attr].Ptr = (void *) (data + offset);

	 arrays[attr].Size = exec->vtx.attrsz[src];
	 arrays[attr].StrideB = exec->vtx.vertex_size * sizeof(GLfloat);
	 arrays[attr].Stride = exec->vtx.vertex_size * sizeof(GLfloat);
	 arrays[attr].Type = GL_FLOAT;
         arrays[attr].Format = GL_RGBA;
	 arrays[attr].Enabled = 1;
         _mesa_reference_buffer_object(ctx,
                                       &arrays[attr].BufferObj,
                                       exec->vtx.bufferobj);
	 arrays[attr]._MaxElement = count; /* ??? */

         _mesa_printf("%s attr %d ptr %x stride %d\n", 
                      __FUNCTION__,
                      attr,
                      arrays[attr].Ptr,
                      arrays[attr].Stride );

         varying_inputs |= 1<<attr;
      }
   }

   _mesa_set_varying_vp_inputs( ctx, varying_inputs );
}


static void st_vbo_exec_vtx_unmap( struct st_vbo_exec_context *exec )
{
   GLenum target = GL_ARRAY_BUFFER_ARB;

   if (exec->vtx.bufferobj->Name) {
      GLcontext *ctx = exec->st_vbo->ctx;

      if(ctx->Driver.FlushMappedBufferRange) {
         GLintptr offset = exec->vtx.buffer_used - exec->vtx.bufferobj->Offset;
         GLsizeiptr length = (exec->vtx.buffer_ptr - exec->vtx.buffer_map) * sizeof(float);

         if(length)
            ctx->Driver.FlushMappedBufferRange(ctx, target,
                                               offset, length,
                                               exec->vtx.bufferobj);
      }

      exec->vtx.buffer_used += (exec->vtx.buffer_ptr -
                                exec->vtx.buffer_map) * sizeof(float);

      assert(exec->vtx.buffer_used <= ST_VBO_VERT_BUFFER_SIZE);
      assert(exec->vtx.buffer_ptr != NULL);

      ctx->Driver.UnmapBuffer(ctx, target, exec->vtx.bufferobj);
      exec->vtx.buffer_map = NULL;
      exec->vtx.buffer_ptr = NULL;
      exec->vtx.max_vert = 0;
   }
}

void st_vbo_exec_vtx_map( struct st_vbo_exec_context *exec )
{
   GLcontext *ctx = exec->st_vbo->ctx;
   GLenum target = GL_ARRAY_BUFFER_ARB;
   GLenum access = GL_READ_WRITE_ARB;
   GLenum usage = GL_STREAM_DRAW_ARB;

   if (exec->vtx.bufferobj->Name == 0)
      return;

   if (exec->vtx.buffer_map != NULL) {
      assert(0);
      exec->vtx.buffer_map = NULL;
      exec->vtx.buffer_ptr = NULL;
   }

   if (ST_VBO_VERT_BUFFER_SIZE > exec->vtx.buffer_used + 1024 &&
       ctx->Driver.MapBufferRange)
   {
      exec->vtx.buffer_map =
         (GLfloat *)ctx->Driver.MapBufferRange(ctx,
                                               target,
                                               exec->vtx.buffer_used,
                                               (ST_VBO_VERT_BUFFER_SIZE -
                                                exec->vtx.buffer_used),
                                               (GL_MAP_WRITE_BIT |
                                                GL_MAP_INVALIDATE_RANGE_BIT |
                                                GL_MAP_UNSYNCHRONIZED_BIT |
                                                MESA_MAP_NOWAIT_BIT),
                                               exec->vtx.bufferobj);
      exec->vtx.buffer_ptr = exec->vtx.buffer_map;
   }

   if (!exec->vtx.buffer_map) {
      exec->vtx.buffer_used = 0;

      ctx->Driver.BufferData(ctx, target,
                             ST_VBO_VERT_BUFFER_SIZE,
                             NULL, usage, exec->vtx.bufferobj);

      exec->vtx.buffer_map =
         (GLfloat *)ctx->Driver.MapBuffer(ctx, target, access, exec->vtx.bufferobj);
      exec->vtx.buffer_ptr = exec->vtx.buffer_map;
   }

   if (0) _mesa_printf("map %d..\n", exec->vtx.buffer_used);
}



/*
 * Do the following:
 *   - unmap_vbo();
 *   - fire primitives to backend()
 *   - if (!unmap)
 *      - maybe get_new_vbo();
 *      - map_vbo();
 */
void st_vbo_exec_vtx_flush( struct st_vbo_exec_context *exec,
                            GLboolean unmap )
{
   if (1)
      st_vbo_exec_debug_verts( exec );

   if (exec->vtx.prim_count) {
      GLcontext *ctx = exec->st_vbo->ctx;

      if (exec->vtx.bufferobj->Name) {
         st_vbo_exec_vtx_unmap( exec );
      }

      if (1) _mesa_printf("%s %d %d\n", __FUNCTION__, exec->vtx.prim_count,
                          exec->vtx.vert_count);

      st_vbo_context(ctx)->draw_prims( ctx,
				       exec->vtx.inputs,
				       exec->vtx.prim,
				       exec->vtx.prim_count,
				       NULL,
				       0,
				       exec->vtx.vert_count - 1);
   }

   /* May have to unmap explicitly if we didn't draw:
    */
   if (unmap) {
      if (exec->vtx.bufferobj->Name &&
          exec->vtx.buffer_map)
         st_vbo_exec_vtx_unmap( exec );

      exec->vtx.max_vert = 0;
   }
   else {
      if (exec->vtx.bufferobj->Name &&
          !exec->vtx.buffer_map)
         st_vbo_exec_vtx_map( exec );

      exec->vtx.max_vert = ((ST_VBO_VERT_BUFFER_SIZE - exec->vtx.buffer_used) /
                            (exec->vtx.vertex_size * sizeof(GLfloat)));
   }


   exec->vtx.buffer_ptr = exec->vtx.buffer_map;
   exec->vtx.prim_count = 0;
   exec->vtx.vert_count = 0;
   exec->vtx.choke_prim = 0;
}
