#
# $Id$
# Copyright (C) 2001 NaN Technologies B.V.

DIR = $(OCGDIR)/sumo
ALLTARGETS = $(OBJS) $(DIR)/$(DEBUG_DIR)particle $(DIR)/$(DEBUG_DIR)particle0

include nan_compile.mk

CPPFLAGS = -I../../include -I../include -I$(NAN_MOTO)/include
CPPFLAGS += -I$(OPENGL_HEADERS)

clean::
	@$(RM) $(DIR)/particle $(DIR)/particle0
	@$(RM) $(DIR)/debug/particle $(DIR)/debug/particle0

LDFLAGS  = -L$(DIR) -L/usr/X11R6/lib
OGL_LDLIBS = -lglut -lGLU -lGL -pthread
LDLIBS   = -lfuzzics -lsolid $(NAN_MOTO)/lib/libmoto.a $(OGL_LDLIBS)

$(DIR)/$(DEBUG_DIR)particle: particle.o $(DIR)/$(DEBUG_DIR)libfuzzics.a $(DIR)/$(DEBUG_DIR)libsolid.a
	$(CCC) $(CCFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

$(DIR)/$(DEBUG_DIR)particle0: particle0.o $(DIR)/$(DEBUG_DIR)libfuzzics.a $(DIR)/$(DEBUG_DIR)libsolid.a
	$(CCC) $(CCFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)
