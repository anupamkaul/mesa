/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 * Copyright (C) 2008-2009  Matthias Hopf
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
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _R600_CONTEXT_H
#define _R600_CONTEXT_H

#include "r600_common.h"
#include "r600_screen.h"

#include "mtypes.h"
#include "radeon_drm.h"

#include "texmem.h"

#include "main/dd.h"
#include "tnl/t_pipeline.h"


/* From http://gcc.gnu.org/onlinedocs/gcc-3.2.3/gcc/Variadic-Macros.html .
   I suppose we could inline this and use macro to fetch out __LINE__ and stuff in case we run into trouble
   with other compilers ... GLUE!
*/
#define WARN_ONCE(a, ...)                                           \
{                                                                   \
    static int warn##__LINE__=1;                                    \
    if(warn##__LINE__)                                              \
    {                                                               \
        fprintf(stderr, "**********WARN_ONCE******************\n"); \
        fprintf(stderr, "File %s function %s line %d\n", __FILE__,  \
                        __FUNCTION__, __LINE__);                    \
        fprintf(stderr,  a, ## __VA_ARGS__);                        \
        fprintf(stderr, "*************************************\n"); \
        warn##__LINE__=0;                                           \
    }                                                               \
}

#if defined(USE_X86_ASM)
#define COPY_DWORDS( dst, src, nr )                                 \
    do {                                                            \
        int __tmp;                                                  \
        __asm__ __volatile__( "rep ; movsl"                         \
        : "=%c" (__tmp), "=D" (dst), "=S" (__tmp)                   \
        : "0" (nr),                                                 \
        "D" ((long)dst),                                            \
        "S" ((long)src) );                                          \
    } while (0)
#else
#define COPY_DWORDS( dst, src, nr )                                 \
    do {                                                            \
        int j;                                                      \
        for ( j = 0 ; j < nr ; j++ )                                \
            dst[j] = ((int *)src)[j];                               \
        dst += nr;                                                  \
    } while (0)
#endif

#define GET_START(dr)                                               \
    (r600GARTOffsetFromVirtual(context, (dr)->address + (dr)->start))

#define R600_FALLBACK_NONE  0
#define R600_FALLBACK_TCL   1
#define R600_FALLBACK_RAST  2

/* TODO: textures */
/* Pretty evil + simple shortcut - max. numbers of textures used simultaneously */
#define NUM_TEXTURES		16



/*
 * State object.
 * State is concentrated into these objects, and only submitted to the HW if necessary.
 */

/* TODO: verify what is necessary, and what can be derived from GLcontext trivially */
/* state: render targets */
typedef struct {
    target_t            rt;
    target_t            depth;
    color_f_t           clear_col;
    float               clear_depth;
} state_target_t;

/* state: viewport + scissors */
typedef struct {
} state_viewport_t;

/* state: modelview, projection, inverse modelview matrices */
typedef struct {
} state_matrices_t;

/* state: depth, alpha, stencil tests */
typedef struct {
    int                 depth_func;			/* something's wrong here, still */
    int                 alpha_func;			/* TODO: see XXX */
    int                 stencil_func;
    float               alpha_ref;
    uint32_t            stencil_ref;
} state_tests_t;

/* state: blending mode */
typedef struct {
    int                 blend_mode;			/* TODO: see XXX */
    color_f_t           blend_col;
} state_blend_t;

/* state: vertex submission format */
/* doesn't emit any commands, but is updated by glColor() etc.,
 * cleared by glBegin(), and used by glEnd(). */
typedef struct {
    /* unless we know how to reliably submit integer coords,
     * we are always float for coords, and possibly uint8_t for color */
    int                 vert_dim;			/* dimensionality: 2, 3, or 4 */
    bool_t              normals;			/* always 3-dimensional */
    bool_t              colors;				/* always 4-dimensional */
    bool_t              colors_float;
    int                 tex_dim[NUM_TEXTURES];		/* dimensionality: 1, 2, 3, or 4 */
} state_vtx_format_t;

/* Vertex buffer / vtx resource */
typedef struct 
{
    int                 id;
    uint32_t            vb_addr;
    uint32_t            vtx_num_entries;
    uint32_t            vtx_size_dw;
    int                 clamp_x;
    int                 format;
    int                 num_format_all;
    int                 format_comp_all;
    int                 srf_mode_all;
    int                 endian;
    int                 mem_req_size;
} vtx_resource_t;

/* Shader */
typedef struct 
{
    uint32_t            shader_addr;
    int                 num_gprs;
    int                 stack_size;
    int                 dx10_clamp;
    int                 fetch_cache_lines;
    int                 clamp_consts;
    int                 export_mode;
    int                 uncached_first_inst;
} shader_config_t;

/* Draw command */
typedef struct 
{
    uint32_t            prim_type;
    uint32_t            vgt_draw_initiator;
    uint32_t            index_type;
    uint32_t            num_instances;
    uint32_t            num_indices;
} draw_config_t;

/* Color buffer / render target */
typedef struct 
{
    int                 id;
    int                 w;
    int                 h;
    uint32_t            base;
    int                 format;
    int                 endian;
    int                 array_mode; // tiling
    int                 number_type;
    int                 read_size;
    int                 comp_swap;
    int                 tile_mode;
    int                 blend_clamp;
    int                 clear_color;
    int                 blend_bypass;
    int                 blend_float32;
    int                 simple_float;
    int                 round_mode;
    int                 tile_compact;
    int                 source_format;
} cb_config_t;

#if 0	/* TODO: textures */
/* state: texture resource */
typedef struct {
    /* TODO: */
} state_tex_resource_t;

/* state: texture sampler */
typedef struct {
    /* TODO: */
} state_tex_sampler_t;
#endif

/* 
 * TODO: Missing: a lot, e.g. line, point modes, edges, stippling, shaders, vertex buffers,
 * user clipplanes, fog, dithering, stencil ... 
 */

struct r600_cmdbuf
{
    int         size;           /* DWORDS allocated for buffer */
    uint32_t    *cmd_buf;       /* pointer to command buffer */
    int         count_used;     /* DWORDS filled so far */
    int         count_reemit;   /* size of re-emission batch */

    int         nDMAindex;
    GLboolean   bUseDMAasIndirect;
};

struct r600_dma_buffer 
{
    int         refcount;       /* the number of retained regions in buf */
    drmBufPtr   buf;
    int         id;
};

/* 
 * A retained region, eg vertices for indexed vertices.
 */
struct r600_dma_region 
{
    struct r600_dma_buffer  *buf;
    char        *address;       /* == buf->address */
    int         start;
    int         end;
    int         ptr;            /* offsets from start of buf */

    int         aos_offset;     /* address in GART memory */
    int         aos_stride;     /* distance between elements, in dwords */
    int         aos_size;       /* number of components (1-4) */
};


struct r600_dma 
{
    /* 
     * Active dma region.  Allocations for vertices and retained
     * regions come from here.  Also used for emitting random vertices,
     * these may be flushed by calling flush_current();
     */
    struct r600_dma_region current;

    void (*flush) (context_t *context);

    char *buf0_address;	/* start of buf[0], for index calcs */

    /* 
     * Number of "in-flight" DMA buffers, i.e. the number of buffers
     * for which a DISCARD command is currently queued in the command buffer.
     */
    GLuint nr_released_bufs;
};

typedef struct chip_object
{
    void      *pvChipObj;

    /* ------------  OUT ------------------- */
    GLboolean (*DestroyChipObj)(void* pvChipObj);

    void      (*InitFuncs)(struct dd_function_table *functions);

    void      (*InitState)(GLcontext * ctx);

    GLuint    (*GetTexObjSize)(void);

    /* ------------  IN  ------------------- */
    void      (*EmitShader)( GLcontext * ctx, 
                             struct r600_dma_region *rvb,
			                 GLvoid * data, 
                             int sizeinDWORD);
    void      (*FreeDmaRegion)( GLcontext * ctx, 
                                struct r600_dma_region *region);
    void      (*EmitVec)(GLcontext * ctx, 
                         struct r600_dma_region *rvb,
			             GLvoid * data, 
                         int size, 
                         int stride, 
                         int count);
    void      (*MemUse)(struct context_s *context, int id);
    void      (*ReleaseArrays)(GLcontext * ctx);
    int       (*FlushCmdBuffer)(GLcontext * ctx);
    GLboolean (*LoadMemSurf)(context_t *context,
                               GLuint     dst_offset, /* gpu addr */
                               GLuint     dst_pitch_in_pixel,                               
                               GLuint     src_width_in_pixel,
                               GLuint     height,
                               GLuint     byte_per_pixel,
                               unsigned char* pSrc); /* source data */    
    GLboolean (*AllocMemSurf)(context_t   *context,
                           void  **ppmemBlock,
                           void  **ppheap,
                           GLuint      *prefered_heap, 
                           GLuint       totalSize);
    
    struct tnl_pipeline_stage **stages;
} chip_object;

#define R600_MAX_AOS_ARRAYS		32 /* TODO : should be VERT_ATTRIB_MAX */

/*
 * Context
 */
struct context_s {
    GLcontext               *ctx; /* Mesa context */
    screen_t                *screen;
    GLboolean               lost_context;
    
    int                     fd; /* DRM's fd == context->screen->driScreen->fd */
    drm_context_t           hwContext;
    drm_hw_lock_t           *hwLock;
    
    __DRIdrawablePrivate    *currentDraw;
    __DRIdrawablePrivate    *currentRead;
    unsigned int            lastStamp;

    /* dma buffer */
    struct r600_dma         dma;
    
    /* user space command buffer */
    struct r600_cmdbuf      cmdbuf;
    struct r600_mem_manager *memManager;
    
    /* Drawable, cliprect and scissor information */
    int                     numClipRects; /* Cliprects for the draw buffer */
    drm_clip_rect_t         *pClipRects;
    bool_t                  reloadContext;
    drm_radeon_sarea_t      *sarea; /* Private SAREA data */

    /* State */
    state_target_t          target;
    state_viewport_t        viewport;
    state_matrices_t        matrices;
    state_tests_t           tests;
    state_blend_t           blend;
    
#if 0	/* TODO: textures */
    state_tex_resource_t tex_resource[NUM_TEXTURES];	/* up to SQ_TEX_RESOURCE_ps_num */
    state_tex_sampler_t tex_sampler[NUM_TEXTURES];	/* up to SQ_TEX_SAMPLER_WORD_ps_num */
#endif

    /* For vertex shader */
    vtx_resource_t          vtx_res[16];
    state_vtx_format_t      vtx_format;
    shader_config_t         vs_config[8];
    shader_config_t         ps_config[8];
    shader_config_t         fs_config[8];

    chip_object             chipobj;

    GLuint                  NewGLState;

    struct r600_dma_region  aos[R600_MAX_AOS_ARRAYS];
    int                     aos_count;

    GLboolean               bEnablePerspective;

    GLint   vport_x; 
    GLint   vport_y;
    GLsizei vport_width;
    GLsizei vport_height;

    /* TEXTURE */
    int              texture_depth; /* driQueryOption */
    driTexHeap      *texture_heaps[RADEON_NR_TEX_HEAPS];
    driTextureObject swapped;
    unsigned         nr_heaps;
};


/*
 * Functions
 */

GLboolean
r600CreateContext (const __GLcontextModes *glVisual,
		   __DRIcontextPrivate *driContextPriv,
		   void *sharedContextPriv);
void
r600DestroyContext (__DRIcontextPrivate * driContextPriv);

GLboolean
r600MakeCurrent (__DRIcontextPrivate  *driContextPriv,
		 __DRIdrawablePrivate *driDrawPriv,
		 __DRIdrawablePrivate *driReadPriv);

GLboolean
r600UnbindContext (__DRIcontextPrivate * driContextPriv);


#endif	/* _R600_SCREEN_H */
