# src/mesa/drivers/dri/r600/Makefile

TOP = ../../../../..
include $(TOP)/configs/current

LIBNAME = r600_dri.so

#MINIGLX_SOURCES = server/radeon_dri.c

#ifeq ($(USING_EGL), 1)
#EGL_SOURCES = server/radeon_egl.c
#endif

COMMON_SOURCES =			\
	../../common/driverfuncs.c	\
	../common/mm.c			\
	../common/utils.c		\
	../common/texmem.c		\
	../common/vblank.c		\
	../common/xmlconfig.c		\
	../common/dri_util.c

DRIVER_SOURCES = 			\
	r600_context.c			\
	r600_id.c			\
	r600_screen.c			\
	r600_span.c			\
	r600_cmdbuf.c			\
	r600_mem.c			\
	r600_emit.c			\
	r700_assembler.c		\
	r700_fragprog.c			\
	r700_oglprog.c			\
	r700_shader.c			\
	r700_shaderinst.c		\
	r700_vertprog.c			\
	r700_chip.c			\
	r700_state.c			\
	r700_tex.c			\
	r700_render.c			\
	r700_clear.c			\
	r700_debug.c			\
	$(EGL_SOURCES)

C_SOURCES = $(COMMON_SOURCES) $(DRIVER_SOURCES)

DRIVER_DEFINES = 

SYMLINKS = \
	server/radeon_dri.h

COMMON_SYMLINKS =

##### TARGETS #####

include ../Makefile.template

server:
	mkdir -p server

$(SYMLINKS): server
	@[ -e $@ ] || ln -sf ../../radeon/$@ server/

$(COMMON_SYMLINKS):
	@[ -e $@ ] || ln -sf ../radeon/$@ ./

symlinks: $(SYMLINKS) $(COMMON_SYMLINKS)
