# $Id: Makefile,v 1.1.2.2.4.1 2003/05/13 16:39:38 dok666 Exp $

# Mesa 3-D graphics library
# Version:  5.0
# Copyright (C) 1995-2002  Brian Paul



MESA = ../../..
default: mga_dri.so 
include $(MESA)/Makefile.include


SHARED_INCLUDES= -I$(MESABUILDDIR) -I$(MESA)/include -I. -I../common -Iserver
MINIGLX_INCLUDES = -I$(MESABUILDDIR)/miniglx 
DRI_INCLUDES = -I$(MESABUILDDIR)/dri

ifeq ($(FULL_DRIVER),true)
DEFINES = \
	-D_HAVE_SWRAST=1 \
	-D_HAVE_SWTNL=1 \
	-D_HAVE_SANITY=1 \
	-D_HAVE_CODEGEN=1 \
	-D_HAVE_LIGHTING=1 \
	-D_HAVE_TEXGEN=1 \
	-D_HAVE_USERCLIP=1 \
	-D_HAVE_FULL_GL=1
else
DEFINES = \
	-D_HAVE_SWRAST=0 \
	-D_HAVE_SWTNL=0 \
	-D_HAVE_SANITY=0 \
	-D_HAVE_CODEGEN=0 \
	-D_HAVE_LIGHTING=0 \
	-D_HAVE_TEXGEN=0 \
	-D_HAVE_USERCLIP=0 \
	-D_HAVE_FULL_GL=0
endif

# The .a files for each mesa module required by this driver:
#
FULL_MESA = 	$(MESABUILDDIR)/swrast_setup/swrast_setup.a \
		$(MESABUILDDIR)/tnl/tnl.a \
		$(MESABUILDDIR)/math/math.a \
		$(MESABUILDDIR)/array_cache/array_cache.a \
		$(MESABUILDDIR)/swrast/swrast.a \
		$(MESABUILDDIR)/mesa.a \
		$(MESABUILDDIR)/math/math.a #kludge

SUBSET_MESA = 	$(MESABUILDDIR)/mesa.a \
		$(MESABUILDDIR)/math/math.a



MINIGLX_SOURCES = server/mga_dri.c 

DRIVER_SOURCES = \
		mgadd.c			\
		mgapixel.c		\
		mgaspan.c		\
		mgatex.c		\
		mga_texstate.c		\
		mgavb.c			\
		mgaioctl.c		\
		mgarender.c		\
		mgastate.c		\
		mgatexmem.c		\
		mgatris.c		\
		mga_xmesa.c		\
		../common/mm.c		\
		../common/texmem.c	\
		../common/utils.c

SUBSET_DRIVER_SOURCES =

FULL_DRIVER_SOURCES =


INCLUDES = $(MINIGLX_INCLUDES) \
	   $(SHARED_INCLUDES)


ifeq ($(FULL_DRIVER),true)
C_SOURCES = $(MINIGLX_SOURCES) \
	    $(FULL_DRIVER_SOURCES) \
	    $(DRIVER_SOURCES) 
MESA_MODULES = $(FULL_MESA)
else
C_SOURCES = $(MINIGLX_SOURCES) \
	    $(SUBSET_DRIVER_SOURCES) \
	    $(DRIVER_SOURCES) 
MESA_MODULES = $(SUBSET_MESA)
endif


ifeq ($(WINDOW_SYSTEM),dri)
WINOBJ=$(MESABUILDDIR)/dri/dri.a
WINLIB=
else
WINOBJ=
WINLIB=-L$(MESA)/src/miniglx
endif

ASM_SOURCES = 
OBJECTS = $(C_SOURCES:.c=.o) \
	  $(ASM_SOURCES:.S=.o) 



##### TARGETS #####


# Build the subset or full driver?
#
mga_dri.so:  $(OBJECTS) $(MESA_MODULES) $(WINOBJ) Makefile
	rm -f $@ && gcc -o $@ -shared $(OBJECTS) $(MESA_MODULES) $(WINOBJ) $(WINLIB) -lGL -lc -lm
	rm -f $(MESA)/lib/mga_dri.so && \
	install mga_dri.so $(MESA)/lib/mga_dri.so

##### DEPENDENCIES #####

-include $(C_SOURCES:.c=.d)
