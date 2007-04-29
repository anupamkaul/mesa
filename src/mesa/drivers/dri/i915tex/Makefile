
TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = i915tex_dri.so

MINIGLX_SOURCES = server/intel_dri.c

DRIVER_SOURCES = \
	intel_prim.c \
	intel_prim_emit.c \
	intel_prim_cull.c \
	intel_prim_clip.c \
	intel_prim_unfilled.c \
	i915_cache.c \
	i915_context.c \
	i915_differencer.c \
	i915_fpc.c \
	i915_fpc_debug.c \
	i915_fpc_emit.c \
	i915_fpc_translate.c \
	i915_program.c \
	i915_state.c \
	i915_state_constants.c \
	i915_state_dynamic.c \
	i915_state_fallback.c \
	i915_state_immediate.c \
	i915_state_map.c \
	i915_state_program.c \
	i915_state_rasterizer.c \
	i915_state_sampler.c \
	i915_state_static.c \
	i915_state_vertex.c \
	i915_tex_layout.c \
	i915_vtbl.c \
	intel_draw.c \
	intel_draw_quads.c \
	intel_classic.c \
	intel_swrast.c \
	intel_vb.c \
	intel_regions.c \
	intel_buffer_objects.c \
	intel_batchbuffer.c \
	intel_mipmap_tree.c \
	intel_tex_layout.c \
	intel_tex_image.c \
	intel_tex_subimage.c \
	intel_tex_copy.c \
	intel_tex_validate.c \
	intel_tex_format.c \
	intel_tex.c \
	intel_metaops.c \
	intel_pixel.c \
	intel_pixel_copy.c \
	intel_pixel_read.c \
	intel_pixel_draw.c \
	intel_buffers.c \
	intel_blit.c \
	intel_context.c \
	intel_ioctl.c \
	intel_rotate.c \
	intel_screen.c \
	intel_span.c \
	intel_state.c \
	intel_state_draw.c \
	intel_state_pipeline.c \
	intel_state_viewport.c \
	intel_fbo.c \
	intel_depthstencil.c \
	intel_batchpool.c

FOO = \
	intel_prim_flatshade.c \
	intel_prim_offset.c \
	intel_prim_twoside.c 

C_SOURCES = \
	$(COMMON_SOURCES) \
	$(COMMON_BM_SOURCES) \
	$(DRIVER_SOURCES)

ASM_SOURCES = 

DRIVER_DEFINES = -I../intel $(shell pkg-config libdrm --atleast-version=2.3.1 \
				&& echo "-DDRM_VBLANK_FLIP=DRM_VBLANK_FLIP")

include ../Makefile.template

intel_tex_layout.o: ../intel/intel_tex_layout.c

symlinks:
