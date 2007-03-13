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
#define I915_CONSTANT_SIZE     (2+(4*I915_MAX_CONSTANT))
#define I915_PROGRAM_SIZE      192


#define I915_NEW_INPUT_SIZES      (INTEL_NEW_DRIVER0<<0)
#define I915_NEW_VERTEX_FORMAT    (INTEL_NEW_DRIVER0<<1)



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





struct i915_context
{
   struct intel_context intel;

   struct i915_fragment_program *fragment_program;

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

      /* For short-circuiting 
       */
      GLfloat last_buf[I915_MAX_CONSTANT][4];
      GLuint last_bufsz;
   } constants;

   struct {
      /* I915_NEW_INPUT_DIMENSIONS 
       */
      GLubyte input_sizes[FRAG_ATTRIB_MAX];

      GLuint LIS2;
      GLuint LIS4;
   } fragprog;
   

   GLuint program_id;
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
 * i915_state.c
 */
extern void i915InitStateFunctions(struct dd_function_table *functions);


/*======================================================================
 * i915_tex.c
 */
extern void i915UpdateTextureState(struct intel_context *intel);
extern void i915InitTextureFuncs(struct dd_function_table *functions);



/*======================================================================
 * i915_program.c
 */
extern void i915InitFragProgFuncs(struct dd_function_table *functions);

/*======================================================================
 * Inline conversion functions.  These are better-typed than the
 * macros used previously:
 */
static INLINE struct i915_context *
i915_context(GLcontext * ctx)
{
   return (struct i915_context *) ctx;
}



#define I915_CONTEXT(ctx)	i915_context(ctx)



#endif
