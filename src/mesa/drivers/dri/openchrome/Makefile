# src/mesa/drivers/dri/unichrome/Makefile

TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = openchrome_dri.so

MINIGLX_SOURCES = server/via_dri.c 

DRIVER_SOURCES = \
	via_buffer_objects.c \
	via_context.c \
	via_depthstencil.c \
	via_fbo.c \
	via_tex.c \
	via_memcpy.c \
	via_meta.c \
	via_render.c \
	via_span.c \
	via_span_texrender.c \
	via_state.c \
	via_texcombine.c \
	via_teximage_accel.c \
	via_tris.c \
	via_pixel.c \
	via_ws_driver.c \
	via_ioctl.c \
	via_screen.c


C_SOURCES = \
	$(COMMON_SOURCES) \
	$(DRIVER_SOURCES)

ASM_SOURCES = 
DRI_LIB_DEPS += `pkg-config --libs libwsbm`
LIBDRM_CFLAGS += `pkg-config --cflags libwsbm`
include ../Makefile.template

symlinks:
