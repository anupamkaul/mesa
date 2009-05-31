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

#include "st_vbo_exec.h"
#include "st_vbo_context.h"



/* As long as 'upgrade' doesn't change the type of the existing
 * attribute, upgrading a vertex is fairly straight-forward.
 */
static INLINE void upgrade_vertex( char *dest,
                                   const char *src,
                                   unsigned old_dwords,
                                   const char *inject,
                                   unsigned inject_dwords,
                                   unsigned inject_dword_offset )
{
   unsigned old_size = old_dwords * sizeof(GLfloat);
   unsigned inject_size = inject_dwords * sizeof(GLfloat);
   unsigned inject_offset = inject_dword_offset * sizeof(GLfloat);

   if (inject_offset != old_size) {
      memcpy(dest + inject_offset + inject_size,
             src + inject_offset,
             old_size - inject_offset);
   }

   memcpy(dest + inject_offset,
          inject,
          inject_size);

   if (dest != src)
      memcpy(dest,
             src,
             inject_offset);
}

/* For each of the four cached vertices, inj
 */
static INLINE void upgrade_attrib( struct st_vbo_exec_context *exec,
                                   unsigned attrib,
                                   unsigned new_attrib_size,
                                   const GLfloat *values )
{
   unsigned old_attrib_size = exec->vtx.attrsz[attrib];
   unsigned extra_dwords = new_attrib_size - old_attrib_size;
   unsigned offset = exec->vtx.attrptr[attrib] - exec->vtx.vertex;
   unsigned i;

   if (0)
      _mesa_printf("%s attr %d sz %d (was %d) offset %d\n",
                   __FUNCTION__,
                   attrib,
                   new_attrib_size,
                   old_attrib_size,
                   offset);

   /* Raise a flag to prevent extend_prim() from succeeding on the
    * next call.  That will force a wrap_prim() and re-emit of
    * duplicated vertices at some point in the future.
    */
   st_vbo_exec_vtx_choke_prim( exec );

   /* In the meantime, we will expand the format of the hot vertex and
    * all the cached vertex slots, so that the re-emitted vertices
    * after the wrap will all be of the new size:
    */
   upgrade_vertex( (char *)exec->vtx.vertex,
                   (const char *)exec->vtx.vertex,
                   exec->vtx.vertex_size,
                   (const char *)&values[old_attrib_size],
                   extra_dwords,
                   offset );

   for (i = 0; i < 4; i++) {
      upgrade_vertex( (char *)exec->vtx.slot[i].vertex,
                      (const char *)exec->vtx.slot[i].vertex,
                      exec->vtx.vertex_size,
                      (const char *)&values[old_attrib_size],
                      extra_dwords,
                      offset );
   }

   exec->vtx.attrsz[attrib] = new_attrib_size;

   /* Nasty loop to update the attrptr values:
    */
   for (i = attrib+1; i < ST_VBO_ATTRIB_MAX; i++) {
      exec->vtx.attrptr[i] += extra_dwords;
   }

   if (0) {
      _mesa_printf("after upgrade, offsets:\n");
      for (i = 0; i < ST_VBO_ATTRIB_MAX; i++) {
         unsigned offset = exec->vtx.attrptr[i] - exec->vtx.vertex;
         _mesa_printf("   attr[%d]: sz %d off %d\n", i,
                      exec->vtx.attrsz[attrib],
                      offset);
      }
   }


   exec->vtx.vertex_size += extra_dwords;
}




/* For a new attribute, after flushing extend each of the four (or
 * fewer) active vertices with the value of the attribute from
 * ctx->current.
 */
static void grow_attrib( struct st_vbo_exec_context *exec,
                         unsigned attrib,
                         unsigned new_attrib_size )
{

   /* Upgrade existing attribute or introduce new one.
    */
   if (exec->vtx.attrsz[attrib] == 0)
   {
      const GLfloat *current = (GLfloat *)exec->st_vbo->currval[attrib].Ptr;
      unsigned dword_offset = 0;
      unsigned i;

      /* Try to keep attribs ordered to match old behaviour.
       */
      for (i = attrib; i > 0; i--) {
         unsigned j = i - 1;
         if (exec->vtx.attrsz[j]) {
            dword_offset = (exec->vtx.attrptr[j] +
                            exec->vtx.attrsz[j] -
                            exec->vtx.vertex);
            break;
         }
      }

      exec->vtx.attrptr[attrib] = exec->vtx.vertex + dword_offset;

      /* Upgrade the zero-sized attribute and fill with values from
       * ctx->Current.
       */
      upgrade_attrib( exec, attrib, new_attrib_size, current );
   }
   else if (new_attrib_size > exec->vtx.attrsz[attrib])
   {
      static const float identity[4] = {0,0,0,1};

      /* Growing an existing attribute.  Upgrade the existing
       * attributes inplace with values from identity[].
       */
      upgrade_attrib( exec, attrib, new_attrib_size, identity );
   }
}

static void shrink_attrib( struct st_vbo_exec_context *exec,
                           GLuint attr,
                           GLuint sz )
{
   static const float identity[4] = {0,0,0,1};
   unsigned i;

   /* New size is smaller - just need to fill in some zeros.  Don't
    * need to flush or wrap.
    */
   for (i = sz ; i <= exec->vtx.attrsz[attr] ; i++)
      exec->vtx.attrptr[attr][i-1] = identity[i-1];
}


void st_vbo_exec_fixup_vertex( struct st_vbo_exec_context *exec,
                               GLuint attr,
                               GLuint sz )
{
   if (sz < exec->vtx.active_sz[attr])
      shrink_attrib( exec, attr, sz );
   else
      grow_attrib( exec, attr, sz );

   exec->vtx.attrsz[attr] = sz;
}
