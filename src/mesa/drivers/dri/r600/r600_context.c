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

/*
 * Authors:
 *   Matthias Hopf
 *   Richard Li <RichardZ.Li@amd.com>, <richardradeon@gmail.com>
 *   CooperYuan <cooper.yuan@amd.com>, <cooperyuan@gmail.com>
 */

#include "r600_context.h"
#include "r600_span.h"
#include "r600_mem.h"

#include "utils.h"
#include "drivers/common/driverfuncs.h"
#include "drm_sarea.h"					/* r600CopyBuffer */

#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"
#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "api_arrayelt.h"
#include "extensions.h"
#include "state.h"

#include "r700_interface.h"

#include "simple_list.h"
#include "xmlpool.h"

#define need_GL_EXT_stencil_two_side
#define need_GL_ARB_multisample
#define need_GL_ARB_point_parameters
#define need_GL_ARB_texture_compression
#define need_GL_ARB_vertex_buffer_object
#define need_GL_ARB_vertex_program
#define need_GL_EXT_blend_minmax
//#define need_GL_EXT_fog_coord
#define need_GL_EXT_multi_draw_arrays
#define need_GL_EXT_secondary_color
#define need_GL_EXT_blend_equation_separate
#define need_GL_EXT_blend_func_separate
#define need_GL_EXT_gpu_program_parameters
#define need_GL_NV_vertex_program
#include "extension_helper.h"

/* richard TODO */
extern void GLAPIENTRY
_mesa_ProgramStringARB(GLenum target, GLenum format, GLsizei len,
                       const GLvoid *string);
/*-----------------*/

// TODO : copied from r300_context.c, need verify for r6/r7
const struct dri_extension r600_card_extensions[] = {
  /* *INDENT-OFF* */
  {"GL_ARB_depth_texture",		NULL},
  {"GL_ARB_fragment_program",		NULL},
  {"GL_ARB_multisample",		NULL},
  {"GL_ARB_multitexture",		NULL},
  {"GL_ARB_point_parameters",		NULL},
  {"GL_ARB_shadow",			NULL},
  {"GL_ARB_shadow_ambient",		NULL},
  {"GL_ARB_texture_border_clamp",	NULL},
  {"GL_ARB_texture_compression",	NULL},
  {"GL_ARB_texture_cube_map",		NULL},
  {"GL_ARB_texture_env_add",		NULL},
  {"GL_ARB_texture_env_combine",	NULL},
  {"GL_ARB_texture_env_crossbar",	NULL},
  {"GL_ARB_texture_env_dot3",		NULL},
  {"GL_ARB_texture_mirrored_repeat",	NULL},
  {"GL_ARB_vertex_buffer_object",	NULL},
  {"GL_ARB_vertex_program",		GL_ARB_vertex_program_functions},
  {"GL_EXT_blend_equation_separate",	NULL},
  {"GL_EXT_blend_func_separate",	NULL},
  {"GL_EXT_blend_minmax",		NULL},
  {"GL_EXT_blend_subtract",		NULL},
//  {"GL_EXT_fog_coord",			GL_EXT_fog_coord_functions },
  {"GL_EXT_multi_draw_arrays",		NULL},
  {"GL_EXT_gpu_program_parameters",     NULL},
  {"GL_EXT_secondary_color", 		NULL},
  {"GL_EXT_shadow_funcs",		NULL},
  {"GL_EXT_stencil_two_side",		NULL},
  {"GL_EXT_stencil_wrap",		NULL},
  {"GL_EXT_texture_edge_clamp",		NULL},
  {"GL_EXT_texture_env_combine", 	NULL},
  {"GL_EXT_texture_env_dot3", 		NULL},
  {"GL_EXT_texture_filter_anisotropic",	NULL},
  {"GL_EXT_texture_lod_bias",		NULL},
  {"GL_EXT_texture_mirror_clamp",	NULL},
  {"GL_EXT_texture_rectangle",		NULL},
  {"GL_ATI_texture_env_combine3",	NULL},
  {"GL_ATI_texture_mirror_once",	NULL},
  {"GL_MESA_pack_invert",		NULL},
  {"GL_MESA_ycbcr_texture",		NULL},
  {"GL_MESAX_texture_float",		NULL},
  {"GL_NV_blend_square",		NULL},
  {"GL_NV_vertex_program",		NULL},
  {"GL_SGIS_generate_mipmap",		NULL},
  {"GL_SGIX_depth_texture",		NULL},
  {NULL,				NULL}
  /* *INDENT-ON* */
};

extern void r600EmitShader( GLcontext * ctx, 
                            struct r600_dma_region *rvb,
			                GLvoid * data, 
                            int sizeinDWORD);
extern void r600FreeDmaRegion(context_t *context, 
                              struct r600_dma_region *region);
extern void r600EmitVec(GLcontext * ctx, 
                        struct r600_dma_region *rvb,
			            GLvoid * data, 
                        int size, 
                        int stride, 
                        int count);
extern void r600ReleaseArrays(GLcontext * ctx);
extern int  r600FlushCmdBuffer(context_t *context);
extern void r600MemUse(context_t *context, int id);

extern int r600FlushIndirectBuffer(context_t *context);

extern const struct tnl_pipeline_stage *r600_pipeline[];

/* glGetString() */
static const GLubyte *
r600GetString (GLcontext *ctx, GLenum name)
{
    R600_CONTEXT;
    
    switch (name) {
    case GL_VENDOR:
	return (GLubyte *) "rhd DRI project";
    case GL_RENDERER:
	switch (context->screen->chip.type & CHIP_TYPE_ARCH_MASK) {
	case CHIP_TYPE_ARCH_R6xx:
	    return (GLubyte *) "R6xx";
	case CHIP_TYPE_ARCH_R7xx:
	    return (GLubyte *) "R7xx";
	default:
	    assert (0);
	}
    }
    return NULL;
}

/* dummy for now. TODO : tex lbj delete func. */
void rDestroyTexObj(void * driverContext, driTextureObject * t)
{
    return;
}

/*
 * API
 */

GLboolean
r600CreateContext (const __GLcontextModes *glVisual,
		   __DRIcontextPrivate *driContextPriv,
		   void *sharedContextPriv)
{
    __DRIscreenPrivate *sPriv  = driContextPriv->driScreenPriv;
    screen_t           *screen = (screen_t *) sPriv->private;
    context_t          *context;
    GLcontext          *ctx, *sharedCtx;
    struct dd_function_table functions;

    unsigned int       ui;

    assert (glVisual);
    assert (screen);

    /* Allocate context */
    if (! (context = CALLOC (sizeof(*context))) )
	return GL_FALSE;

    /* Init default driver functions */
    _mesa_init_driver_functions (&functions);

    context->cmdbuf.bUseDMAasIndirect = GL_FALSE;

    r700InitChipObject(context);  /* let the eag... */

    (context->chipobj.InitFuncs)(&functions);

    context->chipobj.EmitShader     = r600EmitShader;
    context->chipobj.FreeDmaRegion  = r600FreeDmaRegion;
    context->chipobj.EmitVec        = r600EmitVec;
    context->chipobj.ReleaseArrays  = r600ReleaseArrays;
    context->chipobj.LoadMemSurf    = r600LoadMemSurf;
    context->chipobj.AllocMemSurf   = r600AllocMemSurf;
    if(GL_TRUE == context->cmdbuf.bUseDMAasIndirect)
    {
        context->chipobj.FlushCmdBuffer = r600FlushIndirectBuffer;
    }
    else
    {
        context->chipobj.FlushCmdBuffer = r600FlushCmdBuffer;
    }
    context->chipobj.MemUse         = r600MemUse;

    for(ui=0; ui<R600_MAX_AOS_ARRAYS; ui++)
    {
        context->aos[ui].buf = NULL;
    }
    
    /* Fill in additional standard functions. */
    functions.GetString = r600GetString;

    r600MemInit(context);
    
    /* Allocate and initialize the Mesa context */
    if (sharedContextPriv)
        sharedCtx = ((context_t *)sharedContextPriv)->ctx;
    else
        sharedCtx = NULL;
    
    if (! (ctx = _mesa_create_context (glVisual, sharedCtx,
                                      &functions, context)) )
	return GL_FALSE;
    
    context->ctx        = ctx;
    context->screen     = screen;
    context->fd         = sPriv->fd;
    context->hwContext  = driContextPriv->hHWContext;
    context->hwLock     = &sPriv->pSAREA->lock;
    context->sarea      = (drm_radeon_sarea_t *) ((GLubyte *) sPriv->pSAREA +
					     screen->sareaPrivOffset);

    if( (screen->chip.type & CHIP_TYPE_ARCH_MASK) == CHIP_TYPE_ARCH_R7xx) 
    {
        /* use vram only for now */
        context->nr_heaps = 1;
        (void)memset(context->texture_heaps, 0, sizeof(context->texture_heaps));
    	make_empty_list(&context->swapped);
        context->texture_heaps[RADEON_LOCAL_TEX_HEAP] = 
            driCreateTextureHeap(RADEON_LOCAL_TEX_HEAP,   
                                 (void*)context,
                                 (unsigned)screen->texSize[RADEON_LOCAL_TEX_HEAP], 
                                 8,                     /* 256 bytes aligned */
                                 RADEON_NR_TEX_REGIONS, /* 64 regions */
                                 (drmTextureRegionPtr)&context->sarea->tex_list[RADEON_LOCAL_TEX_HEAP],
                                 (unsigned *)&context->sarea->tex_age[RADEON_LOCAL_TEX_HEAP],
                                 (driTextureObject *)&context->swapped,
                                 (unsigned)(context->chipobj.GetTexObjSize)(),
                                 (destroy_texture_object_t *)rDestroyTexObj);
    }

    ctx->Const.MinPointSize   = 0x0001 / 8.0;		/* PA_SU_POINT_SIZE: 12.4 bits radius */
    ctx->Const.MaxPointSize   = 0xffff / 8.0;
    ctx->Const.MinPointSizeAA = 0x0001 / 8.0;
    ctx->Const.MaxPointSizeAA = 0xffff / 8.0;
    ctx->Const.PointSizeGranularity = 0x0001 / 8.0;
    ctx->Const.MinLineWidth   = 0x0001 / 8.0;		/* PA_SU_LINE_CNTL: 12.4 bits halfwidth */
    ctx->Const.MaxLineWidth   = 0xffff / 8.0;
    ctx->Const.MinLineWidthAA = 0x0001 / 8.0;
    ctx->Const.MaxLineWidthAA = 0xffff / 8.0;
    ctx->Const.LineWidthGranularity = 0x0001 / 8.0;

    /* TODO : driQueryOptioni, and check r6/r7 caps */
    context->texture_depth = DRI_CONF_TEXTURE_DEPTH_32;
    ctx->Const.MaxTextureLevels        = 12;     /**< Maximum number of allowed mipmap levels. */
    ctx->Const.Max3DTextureLevels      = 9;      /**< Maximum number of allowed mipmap levels for 3D texture targets. */
    ctx->Const.MaxCubeTextureLevels    = 12;     /**< Maximum number of allowed mipmap levels for GL_ARB_texture_cube_map */
    ctx->Const.MaxArrayTextureLayers   = 64;     /**< Maximum number of layers in an array texture. */
    ctx->Const.MaxTextureRectSize      = 2048;   /* GL_NV_texture_rectangle */
    ctx->Const.MaxTextureCoordUnits    = 8;
    ctx->Const.MaxTextureImageUnits    = 8;
    ctx->Const.MaxTextureUnits         = 8;      /* = MIN(CoordUnits, ImageUnits) */
    ctx->Const.MaxTextureMaxAnisotropy = 16;     /* GL_EXT_texture_filter_anisotropic */
    ctx->Const.MaxTextureLodBias       = 0;      /* GL_EXT_texture_lod_bias */
    ctx->Const.MaxTextureLevels        = 12;	 /* Max width is 8192, but max pitch is 16k -> max 4096 */
    ctx->Const.MaxTextureRectSize      = 2048;

#if 0
    GLuint MaxClipPlanes;
    GLuint MaxLights;
    GLfloat MaxShininess;                        /* GL_NV_light_max_exponent */
    GLfloat MaxSpotExponent;                     /* GL_NV_light_max_exponent */
    struct gl_program_constants VertexProgram;    /* GL_ARB_vertex_program */
    struct gl_program_constants FragmentProgram;  /* GL_ARB_fragment_program */
    /* shared by vertex and fragment program: */
    GLuint MaxProgramMatrices;
    GLuint MaxProgramMatrixStackDepth;
    /* vertex array / buffer object bounds checking */
    GLboolean CheckArrayBounds;
    /* GL_ARB_draw_buffers */
    GLuint MaxDrawBuffers;
    /* GL_OES_read_format */
    GLenum ColorReadFormat;
    GLenum ColorReadType;
    /* GL_EXT_framebuffer_object */
    GLuint MaxColorAttachments;
    GLuint MaxRenderbufferSize;
    /* GL_ARB_vertex_shader */
    GLuint MaxVertexTextureImageUnits;
    GLuint MaxVarying;
#endif

    /* Initialize the software rasterizer and helper modules */
    _swrast_CreateContext  (ctx);
    _vbo_CreateContext     (ctx);
    _tnl_CreateContext     (ctx);
    _swsetup_CreateContext (ctx);
    _swsetup_Wakeup        (ctx);
    _ae_create_context     (ctx);
    
    /* Install the customized pipeline */
    _tnl_destroy_pipeline  (ctx);
    _tnl_install_pipeline(ctx, (const struct tnl_pipeline_stage **)(context->chipobj.stages));

    /* Configure swrast and TNL to match hardware characteristics */
    _swrast_allow_pixel_fog(ctx, GL_FALSE);
    _swrast_allow_vertex_fog(ctx, GL_TRUE);
    _tnl_allow_pixel_fog(ctx, GL_FALSE);
    _tnl_allow_vertex_fog(ctx, GL_TRUE);

    /* TODO: vertex programs */
    ctx->Const.VertexProgram.MaxInstructions = 1024;
    ctx->Const.VertexProgram.MaxNativeInstructions = 1024;
    ctx->Const.VertexProgram.MaxNativeAttribs = 16;	//For init run
    ctx->Const.VertexProgram.MaxTemps = 223; //256 - 1(R0) - 16 ins -16 outs
    ctx->Const.VertexProgram.MaxNativeTemps = 223;          
    ctx->Const.VertexProgram.MaxNativeParameters = 256;	
    ctx->Const.VertexProgram.MaxNativeAddressRegs = 1;

    ctx->Const.FragmentProgram.MaxNativeTemps = 232; //256 - 9 outs - 15 ins
	ctx->Const.FragmentProgram.MaxNativeAttribs = 11;	/* why? copy i915... */
	ctx->Const.FragmentProgram.MaxNativeParameters = 256;
	ctx->Const.FragmentProgram.MaxNativeAluInstructions = 1024;
	ctx->Const.FragmentProgram.MaxNativeTexInstructions = 64; //ONLY limited by inst, almost no
	ctx->Const.FragmentProgram.MaxNativeInstructions = 1024;
	ctx->Const.FragmentProgram.MaxNativeTexIndirections = 4; //PFS_MAX_TEX_INDIRECT; //Need check
	ctx->Const.FragmentProgram.MaxNativeAddressRegs = 0;	/* and these are?? */

	_tnl_ProgramCacheInit(ctx);

	ctx->FragmentProgram._MaintainTexEnvProgram = GL_TRUE;

    driInitExtensions (ctx, r600_card_extensions, GL_FALSE); 

    r600InitSpanFuncs(ctx);

    if(GL_TRUE == context->cmdbuf.bUseDMAasIndirect)
    {
        r600InitIndirectBuffer(context);
    }
    else
    {
        r600InitCmdBuf(context);
    }
/*     r600InitStateObjects (context); */

    TNL_CONTEXT(ctx)->Driver.RunPipeline = _tnl_run_pipeline;

    driContextPriv->driverPrivate = context;

    /* richard TODO */
    ctx->CurrentDispatch->ProgramStringARB = _mesa_ProgramStringARB;
    /*------------- */

    return GL_TRUE;
}

void
r600DestroyContext (__DRIcontextPrivate * driContextPriv)
{
    GET_CURRENT_CONTEXT (ctx);
    context_t *context = (context_t *) driContextPriv->driverPrivate;

    DEBUG_FUNC;
    
    if (ctx == context->ctx) {
	// TODO: Flush
	_mesa_make_current (NULL, NULL, NULL);
    }
    
    r600DestroyCmdBuf(context);
    /* TODO: texture release, gart allocs, cmd bufs, cliprects, option cache */
    r600MemDestroy(context);

    (context->chipobj.DestroyChipObj)(context->chipobj.pvChipObj);

    FREE (context);
}

GLboolean
r600MakeCurrent (__DRIcontextPrivate  *driContextPriv,
		 __DRIdrawablePrivate *driDrawPriv,
		 __DRIdrawablePrivate *driReadPriv)
{
    context_t *context = (context_t *) driContextPriv->driverPrivate;

    DEBUG_FUNC;
    DEBUGF("Context %p Draw %p Read %p\n", driContextPriv, driDrawPriv, driReadPriv);
    
    if (! driContextPriv) {
	_mesa_make_current (NULL, NULL, NULL);
	return GL_TRUE;
    }

    /* TODO: set VBlank */

    if (context->currentDraw != driDrawPriv ||
	context->lastStamp   != driDrawPriv->lastStamp) {
//	context->currentDraw = driDrawPriv;
//	r600SetCliprects (context);
//	r600UpdateViewportOffset (context);		/* TODO: */
    }

    context->currentDraw = driDrawPriv;
    context->currentRead = driReadPriv;

    /* Got drawable, init chip context states. */
    (context->chipobj.InitState)(context->ctx);

    _mesa_make_current (context->ctx,
		       (GLframebuffer *) driDrawPriv->driverPrivate,
		       (GLframebuffer *) driReadPriv->driverPrivate);

    _mesa_update_state (context->ctx);

    return GL_TRUE;
}

GLboolean
r600UnbindContext (__DRIcontextPrivate * driContextPriv)
{
    DEBUG_FUNC;

    return GL_TRUE;
}


