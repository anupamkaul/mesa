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
#include "draw/intel_draw.h"

#include "i915_drm.h"
#include "i830_common.h"

#include "intel_debug.h"

#ifndef DRM_I915_HWZ

#define DRM_I915_HWZ		0x11

#define DRM_I915_HWZ_ALLOC	2
#define DRM_I915_HWZ_RENDER	3

typedef struct drm_i915_hwz {
	unsigned int op;
	union foo {
		struct drm_i915_hwz_alloc {
			unsigned int num_buffers;
			unsigned short x1;
			unsigned short x2;
			unsigned short pitch;
			unsigned short y1;
			unsigned short y2;
		} alloc;
		struct drm_i915_hwz_render {
			unsigned int bpl_num;
			unsigned int batch_start;
			int DR1;
			int DR4;
			unsigned int static_state_offset;
			unsigned int static_state_size;
		} render;
	} arg;
} drm_i915_hwz_t;

#endif

#define DV_PF_555  (1<<8)
#define DV_PF_565  (2<<8)
#define DV_PF_8888 (3<<8)

struct intel_region;
struct intel_context;
struct _DriBufferObject;


struct intel_texture_object;
struct intel_texture_image;
struct intel_frame_tracker;



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
#define INTEL_NEW_FRAG_ATTRIB_SIZES       0x8
#define INTEL_NEW_CONTEXT                 0x10 /* Lost hardware? */
#define INTEL_NEW_FALLBACK                0x20
#define INTEL_NEW_FALLBACK_PRIMS          0x40
#define INTEL_NEW_METAOPS                 0x80 /* not needed? */
#define INTEL_NEW_VBO                     0x100
#define INTEL_NEW_FENCE                   0x200	/* whatever invalidates RELOC's */
#define INTEL_NEW_CBUF                    0x400
#define INTEL_NEW_ZBUF                    0x800
#define INTEL_NEW_WINDOW_DIMENSIONS       0x1000
#define INTEL_NEW_VB_STATE                0x2000 
#define INTEL_NEW_REDUCED_PRIMITIVE       0x4000 /* still needed to turn stipple on/off */
#define INTEL_NEW_CLEAR_PARAMS            0x8000

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

   GLuint clearparams;

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


struct intel_render;


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



      GLuint (*debug_packet)(const GLuint *stream);

      /* Can just return an upper bound: 
       */
      GLuint (*get_hardware_state_size) (struct intel_context *intel);

      void (*emit_hardware_state) (struct intel_context *intel);

      GLboolean (*check_indirect_space) (struct intel_context *intel);

   } vtbl;

   GLint refcount;

   struct intel_context_state state;
   
   struct intel_driver_state driver_state;
   

   struct {
      /* Will be allocated on demand if needed.   
       */
      struct intel_context_state state;
      struct gl_fragment_program *fp, *fp_tex;

      struct intel_region *saved_draw_region;
      struct intel_region *saved_depth_region;

      GLuint restore_draw_mask;
      struct gl_fragment_program *restore_fp;

      struct gl_texture_object *texobj;

      GLboolean active;
   } metaops;


   /* All the known rasterizers:
    */
   struct intel_render *classic;
   struct intel_render *swrender;
   struct intel_render *mixed;
   struct intel_render *swz;
   struct intel_render *hwz;

   /* Current active rasterizer: 
    */
   struct intel_render *render;

   /* Track frame events to help decide which renderer to use:
    */
   struct intel_frame_tracker *ft;

   /* The drawing engine: 
    */
   struct intel_draw *draw;

   /* State to keep it fed: 
    */
   struct intel_draw_state draw_state;

   /* State we get back from it:
    */
   struct intel_draw_vb_state vb_state;
   GLuint fallback_prims:16;

   GLenum hw_reduced_prim;
   GLuint Fallback;


   struct _DriFenceObject *last_swap_fence;
   struct _DriFenceObject *first_swap_fence;

   struct intel_batchbuffer *batch;
   struct intel_vb *vb;



   GLuint ClearColor565;
   GLuint ClearColor8888;


   GLfloat polygon_offset_scale;        /* dependent on depth_scale, bpp */

   GLboolean locked;
   GLboolean contended_lock;


   GLboolean hw_stencil;

   GLboolean strict_conformance;

   /* AGP memory buffer manager:
    */
   struct bufmgr *bm;

   /* Counter to track program string changes:
    */
   GLuint program_id;

   /* Track TNL attrib sizes:
    */
   GLuint frag_attrib_sizes;
   GLuint frag_attrib_varying;

   /* These refer to the current drawing buffer:
    */
   int drawX, drawY;            /**< origin of drawing area within region */
   GLuint numClipRects;         /**< cliprects for drawing */
   drm_clip_rect_t *pClipRects;
   drm_clip_rect_t fboRect;     /**< cliprect for FBO rendering */

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

  /* Rotation. Need to match that of the
   * current screen.
   */

  int width;
  int height;
  int current_rotation;
};


extern char *__progname;


#define SUBPIXEL_X 0.125
#define SUBPIXEL_Y 0.125

#define INTEL_FIREVERTICES(intel) intel->render->flush( intel->render )	

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


/*======================================================================
 * intel_swrast.c 
 */
struct intel_render *intel_create_swrast_render( struct intel_context *intel );


/*======================================================================
 * intel_classic.c 
 */
struct intel_render *intel_create_classic_render( struct intel_context *intel );


/*======================================================================
 * intel_hwz.c 
 */
struct intel_render *intel_create_hwz_render( struct intel_context *intel );

/*======================================================================
 * intel_clears.c 
 */
void intelClear(GLcontext *ctx, GLbitfield mask);
void intelClearColor(GLcontext * ctx, const GLfloat color[4]);



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
