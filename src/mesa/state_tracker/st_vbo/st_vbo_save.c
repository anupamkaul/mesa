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


#include "main/mtypes.h"
#include "main/bufferobj.h"
#include "main/dlist.h"
#include "main/vtxfmt.h"
#include "main/imports.h"

#include "st_vbo_context.h"



static void st_vbo_save_callback_init( GLcontext *ctx )
{
   ctx->Driver.NewList = st_vbo_save_NewList;
   ctx->Driver.EndList = st_vbo_save_EndList;
   ctx->Driver.SaveFlushVertices = st_vbo_save_SaveFlushVertices;
   ctx->Driver.BeginCallList = st_vbo_save_BeginCallList;
   ctx->Driver.EndCallList = st_vbo_save_EndCallList;
   ctx->Driver.NotifySaveBegin = st_vbo_save_NotifyBegin;
}



void st_vbo_save_init( GLcontext *ctx )
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct st_vbo_save_context *save = &st_vbo->save;

   save->ctx = ctx;

   st_vbo_save_api_init( save );
   st_vbo_save_callback_init(ctx);

   {
      struct gl_client_array *arrays = save->arrays;
      memcpy(arrays,      st_vbo->legacy_currval,  16 * sizeof(arrays[0]));
      memcpy(arrays + 16, st_vbo->generic_currval, 16 * sizeof(arrays[0]));
   }

   ctx->Driver.CurrentSavePrimitive = PRIM_UNKNOWN;
}


void st_vbo_save_destroy( GLcontext *ctx )
{
   struct st_vbo_context *st_vbo = st_vbo_context(ctx);
   struct st_vbo_save_context *save = &st_vbo->save;
   GLuint i;

   if (save->prim_store) {
      if ( --save->prim_store->refcount == 0 ) {
         FREE( save->prim_store );
         save->prim_store = NULL;
      }
      if ( --save->vertex_store->refcount == 0 ) {
         _mesa_reference_buffer_object(ctx,
                                       &save->vertex_store->bufferobj, NULL);
         FREE( save->vertex_store );
         save->vertex_store = NULL;
      }
   }

   for (i = 0; i < ST_VBO_ATTRIB_MAX; i++) {
      _mesa_reference_buffer_object(ctx, &save->arrays[i].BufferObj, NULL);
   }
}




/* Note that this can occur during the playback of a display list:
 */
void st_vbo_save_fallback( GLcontext *ctx, GLboolean fallback )
{
   struct st_vbo_save_context *save = &st_vbo_context(ctx)->save;

   if (fallback)
      save->replay_flags |= ST_VBO_SAVE_FALLBACK;
   else
      save->replay_flags &= ~ST_VBO_SAVE_FALLBACK;
}


