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

#ifndef INTELCONTEXT_INC
#define INTELCONTEXT_INC



#include "mtypes.h"
#include "drm.h"
#include "mm.h"
#include "texmem.h"

#include "intel_screen.h"
#include "i915_drm.h"
#include "i830_common.h"
#include "tnl/t_vertex.h"

#define TAG(x) intel##x
#include "tnl_dd/t_dd_vertex.h"
#undef TAG

#define DV_PF_555  (1<<8)
#define DV_PF_565  (2<<8)
#define DV_PF_8888 (3<<8)

struct intel_region;
struct intel_context;
struct _DriBufferObject;


struct intel_texture_object;
struct intel_texture_image;


typedef void (*intel_tri_func) (struct intel_context *, intelVertex *,
                                intelVertex *, intelVertex *);
typedef void (*intel_line_func) (struct intel_context *, intelVertex *,
                                 intelVertex *);
typedef void (*intel_point_func) (struct intel_context *, intelVertex *);

#define INTEL_FALLBACK_DRAW_BUFFER	 0x1
#define INTEL_FALLBACK_READ_BUFFER	 0x2
#define INTEL_FALLBACK_DEPTH_BUFFER      0x4
#define INTEL_FALLBACK_STENCIL_BUFFER    0x8
#define INTEL_FALLBACK_USER		 0x10
#define INTEL_FALLBACK_RENDERMODE	 0x20
#define INTEL_FALLBACK_OTHER    	 0x40

extern void intelFallback(struct intel_context *intel, GLuint bit,
                          GLboolean mode);
#define FALLBACK( intel, bit, mode ) intelFallback( intel, bit, mode )

#define INTEL_WRITE_PART  0x1
#define INTEL_WRITE_FULL  0x2
#define INTEL_READ        0x4



#define INTEL_NEW_MESA                    0x1 /* Mesa state has changed */
#define INTEL_NEW_FRAGMENT_PROGRAM        0x2
#define INTEL_NEW_VERTEX_SIZE             0x4
#define INTEL_NEW_INPUT_SIZES             0x8
#define INTEL_NEW_CONTEXT                 0x10 /* Lost hardware? */
#define INTEL_NEW_REDUCED_PRIMITIVE       0x20
#define INTEL_NEW_FALLBACK                0x40
#define INTEL_NEW_METAOPS                 0x80 /* not needed? */
#define INTEL_NEW_VBO                     0x100
#define INTEL_NEW_FENCE                   0x200	/* whatever invalidates RELOC's */
#define INTEL_NEW_CBUF                    0x400
#define INTEL_NEW_ZBUF                    0x800
#define INTEL_NEW_WINDOW_DIMENSIONS       0x1000

#define INTEL_NEW_DRIVER0                 0x10000

struct intel_state_flags {
   GLuint mesa;
   GLuint intel;
   GLuint extra;
};


struct intel_context_state {
   struct gl_colorbuffer_attrib	*Color;
   struct gl_depthbuffer_attrib	*Depth;
   struct gl_fog_attrib		*Fog;
   struct gl_hint_attrib	*Hint;
   struct gl_light_attrib	*Light;
   struct gl_line_attrib	*Line;
   struct gl_point_attrib	*Point;
   struct gl_polygon_attrib	*Polygon;
   GLuint                       *PolygonStipple;
   struct gl_scissor_attrib	*Scissor;
   struct gl_stencil_attrib	*Stencil;
   struct gl_texture_attrib	*Texture;
   struct gl_transform_attrib	*Transform;
   struct gl_viewport_attrib	*Viewport;
   struct gl_vertex_program_state *VertexProgram; 
   struct gl_fragment_program_state *FragmentProgram;

   GLframebuffer *DrawBuffer;	
   GLframebuffer *ReadBuffer;	
   GLenum RenderMode;

   GLuint _ColorDrawBufferMask0; /* ???  */

   struct intel_region *draw_region; /* INTEL_NEW_CBUF */
   struct intel_region *depth_region; /* INTEL_NEW_ZBUF */


   struct gl_fragment_program *fp;

   /* Indexed rendering support.  GEN3 specific.
    */
   struct _DriBufferObject *vbo;
   GLuint vbo_offset;


   struct intel_state_flags dirty;
};

/* A single atom of derived state
 */
struct intel_tracked_state {
   struct intel_state_flags dirty;
   void (*update)( struct intel_context *intel );
};

struct intel_driver_state {
   struct intel_tracked_state **atoms;
   GLuint nr_atoms;
};



#define INTEL_MAX_FIXUP 64

struct intel_context
{
   GLcontext ctx;               /* the parent class */

   struct
   {
      void (*destroy) (struct intel_context * intel);
      void (*lost_hardware) (struct intel_context * intel);

      void (*render_start) (struct intel_context * intel);
      void (*set_draw_region) (struct intel_context * intel,
                               struct intel_region * draw_region,
                               struct intel_region * depth_region);

      GLuint (*flush_cmd) (void);

      /* Do with metaops: */
      void (*rotate_window) (struct intel_context * intel,
                             __DRIdrawablePrivate * dPriv, GLuint srcBuf);

      void (*assert_not_dirty) (struct intel_context *intel);

   } vtbl;

   GLint refcount;

   struct intel_context_state state;
   
   struct intel_driver_state driver_state;

   struct {
      /* Will be allocated on demand if needed.   
       */
      struct intel_context_state state;
      struct gl_buffer_object *vbo;      
      GLboolean active;
   } metaops;

   
   GLuint Fallback;

   struct _DriFenceObject *last_swap_fence;
   struct _DriFenceObject *first_swap_fence;

   struct intel_batchbuffer *batch;
   struct intel_vb *vb;

   struct
   {
      GLuint id;
      GLuint primitive;
      GLubyte *start_ptr;
      void (*flush) (struct intel_context *);
   } prim;

   GLboolean locked;

   GLuint ClearColor565;
   GLuint ClearColor8888;


   struct tnl_attr_map vertex_attrs[VERT_ATTRIB_MAX];
   GLuint vertex_attr_count;

   GLfloat polygon_offset_scale;        /* dependent on depth_scale, bpp */

   GLboolean hw_stipple;
   GLboolean hw_stencil;

   GLboolean strict_conformance;

   /* AGP memory buffer manager:
    */
   struct bufmgr *bm;


   /* State for intelvb.c and inteltris.c.
    */
   GLuint coloroffset;
   GLuint specoffset;
   GLuint wpos_offset;
   GLuint wpos_size;
   GLuint RenderIndex;
   GLmatrix ViewportMatrix;
   GLenum render_primitive;
   GLenum reduced_primitive;
   GLuint vertex_size;
   GLubyte *verts;              /* points to tnl->clipspace.vertex_buf */


   /* Fallback rasterization functions 
    */
   intel_point_func draw_point;
   intel_line_func draw_line;
   intel_tri_func draw_tri;

   /* These refer to the current drawing buffer:
    */
   int drawX, drawY;            /**< origin of drawing area within region */
   GLuint numClipRects;         /**< cliprects for drawing */
   drm_clip_rect_t *pClipRects;
   drm_clip_rect_t fboRect;     /**< cliprect for FBO rendering */

   int do_irqs;
   GLuint irqsEmitted;
   drm_i915_irq_wait_t iw;

   drm_context_t hHWContext;
   drmLock *driHwLock;
   int driFd;

   __DRIdrawablePrivate *driDrawable;
   __DRIscreenPrivate *driScreen;
   intelScreenPrivate *intelScreen;
   drmI830Sarea *sarea;

   GLuint lastStamp;

   /**
    * Configuration cache
    */
   driOptionCache optionCache;

   /* VBI
    */
   GLuint vbl_seq;
   GLuint vblank_flags;
   int64_t swap_ust;
   int64_t swap_missed_ust;
   GLuint swap_count;
   GLuint swap_missed_count;
   GLuint swap_scheduled;

   /* Rotation. Need to match that of the
    * current screen.
    */

   int width;
   int height;
   int current_rotation;
};

/* These are functions now:
 */
void LOCK_HARDWARE( struct intel_context *intel );
void UNLOCK_HARDWARE( struct intel_context *intel );

extern char *__progname;


#define SUBPIXEL_X 0.125
#define SUBPIXEL_Y 0.125

#define INTEL_FIREVERTICES(intel)		\
do {						\
   if ((intel)->prim.flush)			\
      (intel)->prim.flush(intel);		\
} while (0)

/* ================================================================
 * Color packing:
 */

#define INTEL_PACKCOLOR4444(r,g,b,a) \
  ((((a) & 0xf0) << 8) | (((r) & 0xf0) << 4) | ((g) & 0xf0) | ((b) >> 4))

#define INTEL_PACKCOLOR1555(r,g,b,a) \
  ((((r) & 0xf8) << 7) | (((g) & 0xf8) << 2) | (((b) & 0xf8) >> 3) | \
    ((a) ? 0x8000 : 0))

#define INTEL_PACKCOLOR565(r,g,b) \
  ((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))

#define INTEL_PACKCOLOR8888(r,g,b,a) \
  ((a<<24) | (r<<16) | (g<<8) | b)



/* ================================================================
 * From linux kernel i386 header files, copes with odd sizes better
 * than COPY_DWORDS would:
 * XXX Put this in src/mesa/main/imports.h ???
 */
#if defined(i386) || defined(__i386__)
static INLINE void *
__memcpy(void *to, const void *from, size_t n)
{
   int d0, d1, d2;
   __asm__ __volatile__("rep ; movsl\n\t"
                        "testb $2,%b4\n\t"
                        "je 1f\n\t"
                        "movsw\n"
                        "1:\ttestb $1,%b4\n\t"
                        "je 2f\n\t"
                        "movsb\n" "2:":"=&c"(d0), "=&D"(d1), "=&S"(d2)
                        :"0"(n / 4), "q"(n), "1"((long) to), "2"((long) from)
                        :"memory");
   return (to);
}
#else
#define __memcpy(a,b,c) memcpy(a,b,c)
#endif



/* ================================================================
 * Debugging:
 */
#if defined(DEBUG)
#define DO_DEBUG 1
extern int INTEL_DEBUG;
#else
#define INTEL_DEBUG		0
#endif

#define DEBUG_TEXTURE	0x1
#define DEBUG_STATE	0x2
#define DEBUG_IOCTL	0x4
#define DEBUG_BLIT	0x8
#define DEBUG_MIPTREE   0x10
#define DEBUG_FALLBACKS	0x20
#define DEBUG_VERBOSE	0x40
#define DEBUG_BATCH     0x80
#define DEBUG_PIXEL     0x100
#define DEBUG_BUFMGR    0x200
#define DEBUG_REGION    0x400
#define DEBUG_FBO       0x800
#define DEBUG_LOCK      0x1000
#define DEBUG_IDX       0x2000
#define DEBUG_TRI       0x4000

#define DBG(...)  do { if (INTEL_DEBUG & FILE_DEBUG_FLAG) _mesa_printf(__VA_ARGS__); } while(0)


#define PCI_CHIP_845_G			0x2562
#define PCI_CHIP_I830_M			0x3577
#define PCI_CHIP_I855_GM		0x3582
#define PCI_CHIP_I865_G			0x2572
#define PCI_CHIP_I915_G			0x2582
#define PCI_CHIP_I915_GM		0x2592
#define PCI_CHIP_I945_G			0x2772
#define PCI_CHIP_I945_GM		0x27A2


/* ================================================================
 * intel_context.c:
 */

extern GLboolean intelInitContext(struct intel_context *intel,
                                  const __GLcontextModes * mesaVis,
                                  __DRIcontextPrivate * driContextPriv,
                                  void *sharedContextPrivate,
                                  struct dd_function_table *functions);

extern void intelGetLock(struct intel_context *intel, GLuint flags);

extern void intelFinish(GLcontext * ctx);
extern void intelFlush(GLcontext * ctx);

extern void intelInitDriverFunctions(struct dd_function_table *functions);

void intel_lost_hardware( struct intel_context *intel );

/* ================================================================
 * intel_state.c:
 */
extern void intel_emit_state( struct intel_context *intel );

/*======================================================================
 * Inline conversion functions.  
 * These are better-typed than the macros used previously:
 */
static INLINE struct intel_context *
intel_context(GLcontext * ctx)
{
   return (struct intel_context *) ctx;
}

static INLINE struct intel_texture_object *
intel_texture_object(struct gl_texture_object *obj)
{
   return (struct intel_texture_object *) obj;
}

static INLINE struct intel_texture_image *
intel_texture_image(struct gl_texture_image *img)
{
   return (struct intel_texture_image *) img;
}

extern struct intel_renderbuffer *intel_renderbuffer(struct gl_renderbuffer
                                                     *rb);


#endif
