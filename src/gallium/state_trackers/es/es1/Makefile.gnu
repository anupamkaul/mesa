#----------------------------------------------------------------------------
#
# Copyright (2008).  Intel Corporation.
#
# Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
# develop this driver.
#
# The source code contained or described herein and all documents
# related to the source code ("Material") are owned by Intel
# Corporation or its suppliers or licensors.  Title to the Material
# remains with Intel Corporation or it suppliers and licensors.  The
# Material contains trade secrets and proprietary and confidential 
# information of Intel or its suppliers and licensors.  The Material is
# protected by worldwide copyright and trade secret laws and
# treaty provisions.  No part of the Material may be used, copied,
# reproduced, modified, published, uploaded, posted, transmitted,
# distributed, or disclosed in any way without Intels prior express
# written permission.
#
# No license under any patent, copyright, trade secret or other
# intellectual property right is granted to or conferred upon you by
# disclosure or delivery of the Materials, either expressly, by
# implication, inducement, estoppel or otherwise.  Any license
# under such intellectual property rights must be express
# and approved by Intel in writing.
#
#----------------------------------------------------------------------------
LIB     = gallium_es1
SUBLIB  = gallium_es1
SOURCES = st_es1_context.c\
	  st_es1_drawtex.c\
	  st_es1_generated.c\
	  st_es1_getproc_gen.c\
	  st_es1_get.c\
	  st_es1_getstring.c\
	  st_es1_stubs.c
HEADERS = 

PROJECT_INCLUDES = \
	../common \
	../mesa \
	../mesa/state_tracker \
	$(GALLIUM)/include

include ../Makefile.include
