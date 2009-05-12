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


LINK = ln -s


INCLUDE_DIRS = \
        -I$(GALLIUM)/include \
        -I. \
        -I./main \
        -I$(GALLIUM)/src/gallium/include \
        -I$(GALLIUM)/src/gallium/drivers \
        -I$(GALLIUM)/src/gallium/auxiliary

MESA_SUBDIRS = \
	glapi main math vbo \
	shader shader/slang shader/slang/library shader/grammar \
	state_tracker


SPECIAL_SYMLINKS = shader/grammar/grammar.c


# make .o file from .c file
.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) -UIN_DRI_DRIVER $< -o $@


# create symlink for .c file
%.c:
	$(LINK) $(GALLIUM)/src/mesa/$@ $@

# create symlink for .h file
%.h:
	$(LINK) $(GALLIUM)/src/mesa/$@ $@



.PHONY: default
default: $(MESA_SUBDIRS) symlinks libmesa.a


libmesa.a: $(OBJECTS) $(GALLIUM_LIBS)
	$(TOP)/bin/mklib -o mesa -static $(OBJECTS) $(GALLIUM_LIBS)



# Create all the symlinks back to Mesa source files that we'll use as-is
.PHONY: symlinks
symlinks: $(SPECIAL_SYMLINKS)
	@ for file in $(SYMLINKS) ; do \
		if [ ! -h $$file ] ; then \
			echo "Making symlink to" $$file ; \
			rm -f $$file ; \
			$(LINK) $(GALLIUM)/src/mesa/$$file $$file ; \
		fi \
	done


# Remove symlinks for 'make clean'
.PHONY: rm-symlinks
rm-symlinks:
	for file in $(SYMLINKS) ; do \
		rm -f $$file ; \
	done


# Create the Mesa subdirs as needed
$(MESA_SUBDIRS):
	mkdir $@


# special-case
shader/grammar/grammar.c:
	$(LINK) $(GALLIUM)/src/mesa/$@ $@

main/get.c: main/get_gen.py
	python main/get_gen.py > main/get.c



.PHONY: clean
clean: rm-symlinks
	-rm -f */*.o */*/*.o
	-rm -f depend depend.bak


depend: $(SOURCES)
	@ echo "running $(MKDEP)"
	@ touch depend
	@$(MKDEP) $(MKDEP_OPTIONS) $(DEFINES) $(INCLUDE_DIRS) $(SOURCES) \
		> /dev/null 2>/dev/null


-include depend
