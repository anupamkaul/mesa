# $Id: Makefile,v 1.1.2.1 2003/04/02 00:19:09 jrfonseca Exp $

# Mesa 3-D graphics library
# Version:  5.0
# Copyright (C) 1995-2002  Brian Paul


default: libmesacxx.a 

MESA = ../../..

DEFINES =
INCLUDES = -I$(MESA)/include -I$(MESA)/src -I.

CPPFLAGS = $(DEFINES) $(INCLUDES)

SOURCES = \
	mesa.cxx

OBJECTS := $(SOURCES)
OBJECTS := $(OBJECTS:.c=.o)
OBJECTS := $(OBJECTS:.cxx=.o)
OBJECTS := $(OBJECTS:.S=.o)
OBJECTS := $(filter %.o,$(OBJECTS))


##### TARGETS #####

libmesacxx.a: $(OBJECTS)
	$(AR) ruvs $@ $?

clean:
	-rm -f *.d *.o *.a

##### DEPENDENCIES #####

-include $(OBJECTS:.o=.d)

%.d: %.cxx
#	touch $@ ; rm -f $(<:.cxx=.o)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -M $< > $@

##### RULES #####

%.o: %.cxx
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $<

