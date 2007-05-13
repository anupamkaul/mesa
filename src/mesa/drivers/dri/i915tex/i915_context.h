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

/* Dirty flags for software state update:
 */
#define I915_NEW_DYNAMIC_INDIRECT          (INTEL_NEW_DRIVER0<<0)
#define I915_NEW_VERTEX_FORMAT             (INTEL_NEW_DRIVER0<<1)
#define I915_NEW_POLY_STIPPLE_FALLBACK     (INTEL_NEW_DRIVER0<<2)
#define I915_NEW_LOST_CACHE                (INTEL_NEW_DRIVER0<<3)

/* Dirty flags for hardware emit
 */
#define I915_HW_INDIRECT          (1<<0)
#define I915_HW_IMMEDIATE         (1<<1)



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
#define I915_DYNAMIC_DEPTHSCALE_0 1 /* just the header */
#define I915_DYNAMIC_DEPTHSCALE_1 2 
#define I915_DYNAMIC_IAB          3
#define I915_DYNAMIC_BC_0         4 /* just the header */
#define I915_DYNAMIC_BC_1         5
#define I915_DYNAMIC_BFO_0        6 
#define I915_DYNAMIC_BFO_1        7
#define I915_DYNAMIC_STP_0        8 
#define I915_DYNAMIC_STP_1        9 
#define I915_MAX_DYNAMIC          10


#define I915_IMMEDIATE_S0         0
#define I915_IMMEDIATE_S1         1
#define I915_IMMEDIATE_S2         2
#define I915_IMMEDIATE_S3         3
#define I915_IMMEDIATE_S4         4
#define I915_IMMEDIATE_S5         5
#define I915_IMMEDIATE_S6         6
#define I915_IMMEDIATE_S7         7
#define I915_MAX_IMMEDIATE        8

/* These must mach the order of LI0_STATE_* bits, as they will be used
 * to generate hardware packets:
 */
#define I915_CACHE_STATIC         0 
#define I915_CACHE_DYNAMIC        1 /* handled specially */
#define I915_CACHE_SAMPLER        2
#define I915_CACHE_MAP            3
#define I915_CACHE_PROGRAM        4
#define I915_CACHE_CONSTANTS      5
#define I915_MAX_CACHE            6


struct i915_cache_context;

/* Use to calculate differences between state emitted to hardware and
 * current driver-calculated state.  
 */
struct i915_state 
{
   GLuint immediate[I915_MAX_IMMEDIATE];
   GLuint offsets[I915_MAX_CACHE];
   GLuint sizes[I915_MAX_CACHE];

   /* Something for vbo:
    */
   struct _DriBufferObject *vbo;
   
   GLuint id;			/* track lost context events */
};


struct i915_context
{
   struct intel_context intel;
   struct i915_cache_context *cctx;

   /* Current hardware vertex format:
    */
   struct vf_attr_map vertex_attrs[FRAG_ATTRIB_MAX];
   GLuint vertex_attr_count;


   struct {
      /* Regions aren't actually that appropriate here as the memory may
       * be from a PBO or FBO.  Just use the buffer id.  Will have to do
       * this for draw and depth for FBO's...
       */
      struct _DriBufferObject *tex_buffer[I915_TEX_UNITS];
      GLuint tex_offset[I915_TEX_UNITS];
   } state;


   struct {
      GLuint hardware[I915_MAX_DYNAMIC];
      GLuint current[I915_MAX_DYNAMIC];
      
      GLuint *ptr;
      GLuint offset;
   } dynamic;


   struct {
      struct intel_tracked_state tracked_state;
   } constants;

   struct {
      /* I915_NEW_VERTEX_FORMAT 
       */
      GLuint LIS2;
      GLuint LIS4;
      GLuint attrs;
   } vertex_format;
   
   /* The current, desired hardware state as calcuated by the state
    * tracker.  It is up to the render engine or other user of state
    * to figure out how to get this into hardware, and to track when
    * hardware state is lost:
    */
   struct i915_state current;
   GLuint hardware_dirty;

   /* Misc flags: 
    */
   GLboolean fallback_on_poly_stipple;
};



/*======================================================================
 * i915_vtbl.c
 */
extern void i915InitVtbl(struct i915_context *i915);


/*======================================================================
 * i915_debug.c
 */
GLboolean i915_debug_packet( struct debug_stream *stream );


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
 * i915_differencer.c
 */
extern void i915_init_differencer( struct i915_context *i915 );



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
