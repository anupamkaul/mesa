# src/gallium/state_trackers/es/Makefile

# Build the ES state tracker library (libGLESv1_CM.so)
# This consists of core Mesa, plus GL/gallium state tracker plus
# a thin "API veneer" layer that translates GL ES entrypoints into
# mesa calls.  Ex: glEnable() -> _mesa_Enable()


TOP = ../../../..
include $(TOP)/configs/current

# All the functions in here that include GLES/gl.h or GLES2/gl2.h
# will expect to have __GL_EXPORTS defined.  Rather than define this
# for each file that needs it, define it here for all files, even
# though it's superfluous for many of the files here.
DEFINES += -D__GL_EXPORTS

PYTHON = python
PFLAGS = -tt

GLES_1_VERSION_MAJOR = 1
GLES_1_VERSION_MINOR = 1
GLES_1_VERSION_PATCH = 0

GLES_2_VERSION_MAJOR = 2
GLES_2_VERSION_MINOR = 0
GLES_2_VERSION_PATCH = 0


# Maybe move these into configs/default:
GLES_1_LIB = GLESv1_CM
GLES_1_LIB_NAME = lib$(GLES_1_LIB).so
GLES_2_LIB = GLESv2
GLES_2_LIB_NAME = lib$(GLES_2_LIB).so


INCLUDE_DIRS = \
	-Ies-common \
	-Imesa \
	-I$(TOP)/include

ES1_SOURCES = \
	es1/st_es1_context.c \
	es1/st_es1_drawtex.c \
	es1/st_es1_generated.c \
	es1/st_es1_get.c \
	es1/st_es1_getproc_gen.c \
	es1/st_es1_getstring.c \
	es1/st_es1_stubs.c \
	es-common/st_cpaltex.c \
	es-common/st_glapi.c \
	es-common/st_query_matrix.c \
	es-common/st_teximage.c


ES1_OBJECTS = $(ES1_SOURCES:.c=.o)

ES2_SOURCES = \
	es2/st_es2_context.c \
	es2/st_es2_generated.c \
	es2/st_es2_getproc_gen.c \
	es2/st_es2_get.c \
	es2/st_es2_getstring.c \
	es2/st_es2_stubs.c \
	es-common/st_cpaltex.c \
	es-common/st_fbo.c \
	es-common/st_glapi.c \
	es-common/st_teximage.c

ES2_OBJECTS = $(ES2_SOURCES:.c=.o)



# we only need the gallium libs that the state trackers directly use:
GALLIUM_LIBS = \
	$(TOP)/src/gallium/auxiliary/cso_cache/libcso_cache.a \
	$(TOP)/src/gallium/auxiliary/rtasm/librtasm.a \
	$(TOP)/src/gallium/auxiliary/tgsi/libtgsi.a \
	$(TOP)/src/gallium/auxiliary/util/libutil.a

LIBS = mesa/libmesa.a $(GALLIUM_LIBS)

SYS_LIBS = -lm -pthread


.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) $< -o $@


# Default: make both GL ES 1.1 and GL ES 2.0 libraries
default: subdirs $(TOP)/$(LIB_DIR)/$(GLES_1_LIB_NAME) $(TOP)/$(LIB_DIR)/$(GLES_2_LIB_NAME)

# Make the shared libs
$(TOP)/$(LIB_DIR)/$(GLES_1_LIB_NAME): $(ES1_OBJECTS) $(LIBS)
	$(TOP)/bin/mklib -o $(GLES_1_LIB) \
		-major $(GLES_1_VERSION_MAJOR) \
		-minor $(GLES_1_VERSION_MINOR) \
		-patch $(GLES_1_VERSION_PATCH) \
		-install $(TOP)/$(LIB_DIR) \
		$(ES1_OBJECTS) \
		-Wl,--whole-archive $(LIBS) -Wl,--no-whole-archive $(SYS_LIBS)

$(TOP)/$(LIB_DIR)/$(GLES_2_LIB_NAME): $(ES2_OBJECTS) $(LIBS)
	$(TOP)/bin/mklib -o $(GLES_2_LIB) \
		-major $(GLES_2_VERSION_MAJOR) \
		-minor $(GLES_2_VERSION_MINOR) \
		-patch $(GLES_2_VERSION_PATCH) \
		-install $(TOP)/$(LIB_DIR) \
		$(ES2_OBJECTS) \
		-Wl,--whole-archive $(LIBS) -Wl,--no-whole-archive $(SYS_LIBS)


# Make the python-generated files
es1/st_es1_generated.c: APIspec.txt es_generator.py apiutil.py es1_special
	$(PYTHON) $(PFLAGS) es_generator.py -S APIspec.txt -V GLES1.1 > $@

es2/st_es2_generated.c: APIspec.txt es_generator.py apiutil.py es2_special
	$(PYTHON) $(PFLAGS) es_generator.py -S APIspec.txt -V GLES2.0 > $@

es1/st_es1_getproc_gen.c: APIspec.txt es_getproc_gen.py apiutil.py
	$(PYTHON) $(PFLAGS) es_getproc_gen.py -S APIspec.txt -V GLES1.1 > $@

es2/st_es2_getproc_gen.c: APIspec.txt es_getproc_gen.py apiutil.py
	$(PYTHON) $(PFLAGS) es_getproc_gen.py -S APIspec.txt -V GLES2.0 > $@

es1/st_es1_get.c: get_gen.py
	$(PYTHON) $(PFLAGS) get_gen.py 1 > $@

es2/st_es2_get.c: get_gen.py
	$(PYTHON) $(PFLAGS) get_gen.py 2 > $@



subdirs:
	(cd mesa && make)


clean:
	-rm -f *.o *~
	-rm -f $(TOP)/$(LIB_DIR)/$(GLES_1_LIB_NAME)* $(TOP)/$(LIB_DIR)/$(GLES_2_LIB_NAME)*
	-rm -f es*/st_es*_generated.c
	-rm -f es*/st_es*_getproc_gen.c
	-rm -f es*/st_es*_get.c
	-rm -f *.pyc
