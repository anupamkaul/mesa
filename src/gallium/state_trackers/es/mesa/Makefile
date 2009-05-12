# src/gallium/state_trackers/es/mesa/Makefile

# This makefile builds a libmesa.a archive that consists of the Mesa
# objects that are needed for OpenGL ES.
# We basically symlink the Mesa sources into this directory file by file,
# omitting the sources we don't need for ES (or are replaced by new versions
# for ES).

# The libmesa.a that we end up with will be used by ../Makefile to build
# an OpenGL ES API library such as libGLESv1_CM.so



TOP = ../../../../..
include $(TOP)/configs/current

include sources.mak


ROOT = `(cd ${TOP} && pwd )`

LINK = ln -s


INCLUDE_DIRS = \
        -I$(TOP)/include \
        -I. \
        -I./main \
        -I$(TOP)/src/gallium/include \
        -I$(TOP)/src/gallium/drivers \
        -I$(TOP)/src/gallium/auxiliary

MESA_SUBDIRS = \
	glapi \
	main \
	math \
	shader \
	shader/grammar \
	shader/slang \
	shader/slang/library \
	state_tracker \
	vbo


SPECIAL_SYMLINKS = shader/grammar/grammar.c


# make .o file from .c file
.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) -UIN_DRI_DRIVER $< -o $@


# Default rule: create the libmesa.a library
.PHONY: default
default: $(MESA_SUBDIRS) $(SYMLINKS) $(SPECIAL_SYMLINKS) libmesa.a


# Create the Mesa subdirs as needed
$(MESA_SUBDIRS):
	mkdir $@


# Create the libmesa.a library (common code for ES1 and ES2 libs)
libmesa.a: $(OBJECTS) $(GALLIUM_AUXILIARIES)
	@echo "Making libmesa.a"
	@ $(TOP)/bin/mklib -o mesa -static $(OBJECTS) $(GALLIUM_LIBS)


# Create symlinks back to Mesa source files that we'll use as-is
$(SYMLINKS) $(SPECIAL_SYMLINKS):
	@ if [ ! -h $@ ] ; then \
		echo "Making symlink to" $@ ; \
		rm -f $@ ; \
		$(LINK) $(ROOT)/src/mesa/$@ $@ ; \
	fi


# Special case file, python-generated
main/get.c: main/get_gen.py
	python main/get_gen.py > main/get.c



.PHONY: clean
clean:
	rm -f $(SYMLINKS) $(SPECIAL_SYMLINKS)
	-rm -f */*.o */*/*.o
	-rm -f depend depend.bak
	-rm -f libmesa.a
	-rm -f *~


depend: $(SOURCES)
	@ echo "running $(MKDEP)"
	@ touch depend
	@$(MKDEP) $(MKDEP_OPTIONS) $(DEFINES) $(INCLUDE_DIRS) $(SOURCES) \
		> /dev/null 2>/dev/null


-include depend
