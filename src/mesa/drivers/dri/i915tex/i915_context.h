 /**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#ifndef I915CONTEXT_INC
#define I915CONTEXT_INC

#include "intel_context.h"


#define I915_MAX_CONSTANT      32
#define I915_PROGRAM_SIZE      192


#define I915_NEW_INPUT_SIZES       (INTEL_NEW_DRIVER0<<0)
#define I915_NEW_VERTEX_FORMAT     (INTEL_NEW_DRIVER0<<1)
#define I915_NEW_DYNAMIC_INDIRECT  (INTEL_NEW_DRIVER0<<2)



/* Hardware version of a parsed fragment program.  "Derived" from the
 * mesa fragment_program struct.
 */
struct i915_fragment_program
{
   struct gl_fragment_program Base;
   GLboolean error;             /* If program is malformed for any reason. */

   GLuint    id;		/* String id */
   GLboolean translated;

   /* Decls + instructions: 
    */
   GLuint program[I915_PROGRAM_SIZE];
   GLuint program_size;
   
   /* Constant buffer:
    */
   GLfloat constant[I915_MAX_CONSTANT][4];
   GLuint nr_constants;

   /* Some of which are parameters: 
    */
   struct
   {
      GLuint reg;               /* Hardware constant idx */
      const GLfloat *values;    /* Pointer to tracked values */
   } param[I915_MAX_CONSTANT];
   GLuint nr_params;

   GLuint param_state;
   GLuint wpos_tex;
};




#define I915_TEX_UNITS 8

#define I915_DYNAMIC_MODES4       0
#define I915_DYNAMIC_DEPTHSCALE_0 1 
#define I915_DYNAMIC_DEPTHSCALE_1 2 
#define I915_DYNAMIC_IAB          3
#define I915_DYNAMIC_BC_0         4
#define I915_DYNAMIC_BC_1         5
#define I915_DYNAMIC_BFO_0        6
#define I915_DYNAMIC_BFO_1        7
#define I915_DYNAMIC_SIZE         8


struct i915_cache_context;


struct i915_context
{
   struct intel_context intel;
   struct i915_cache_context *cctx;

   struct {
      /* Regions aren't actually that appropriate here as the memory may
       * be from a PBO or FBO.  Just use the buffer id.  Will have to do
       * this for draw and depth for FBO's...
       */
      struct _DriBufferObject *tex_buffer[I915_TEX_UNITS];
      GLuint tex_offset[I915_TEX_UNITS];
   } state;

   struct {
      struct intel_tracked_state tracked_state;
   } constants;

   struct {
      /* I915_NEW_VERTEX_FORMAT 
       */
      GLuint LIS2;
      GLuint LIS4;
   } vertex_format;
   
   /* Used for short-circuiting packets.  Won't work for packets
    * containing relocations.  This is zero'd out after lost_context
    * events.
    */
   struct {
      GLuint buf[I915_DYNAMIC_SIZE];
      GLboolean done_reset;
   } dyn_indirect;

};



/*======================================================================
 * i915_vtbl.c
 */
extern void i915InitVtbl(struct i915_context *i915);


/*======================================================================
 * i915_context.c
 */
extern GLboolean i915CreateContext(const __GLcontextModes * mesaVis,
                                   __DRIcontextPrivate * driContextPriv,
                                   void *sharedContextPrivate);

/*======================================================================
 * i915_program.c
 */
extern void i915InitFragProgFuncs(struct dd_function_table *functions);



/*======================================================================
 * Inline conversion functions.  
 */
static INLINE struct i915_context *
i915_context( GLcontext *ctx )
{
   return (struct i915_context *) ctx;
}

static INLINE struct i915_fragment_program *
i915_fragment_program( struct gl_fragment_program *fp )
{
   return (struct i915_fragment_program *) fp;
}



#endif
