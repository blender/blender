#
# $Id$
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****
# iksolver test makefile.
#

LIBNAME = iksolver
SOURCEDIR = intern/$(LIBNAME)/test
DIR = $(OCGDIR)/$(SOURCEDIR)

include nan_compile.mk

DIRS = ik_glut_test

include nan_subdirs.mk

include nan_link.mk

LIBS = $(OCGDIR)/intern/$(LIBNAME)/test/ik_glut_test/intern/$(DEBUG_DIR)libintern.a
LIBS +=  $(OCGDIR)/intern/$(LIBNAME)/test/ik_glut_test/common/$(DEBUG_DIR)libcommon.a
LIBS += $(OCGDIR)/intern/$(LIBNAME)/$(DEBUG_DIR)libiksolver.a

SLIBS += $(NAN_MOTO)/lib/$(DEBUG_DIR)libmoto.a

ifeq ($(OS),$(findstring $(OS), "darwin linux freebsd openbsd"))
    LLIBS = -L/usr/X11R6/lib -lglut -pthread
endif

ifeq ($(OS),$(findstring $(OS), "solaris"))
    LLIBS = -L/usr/openwin/lib -lglut -lX11 -lGL -lGLU -lXmu
endif

all debug:: $(LIBS) $(DIR)/$(DEBUG_DIR)iksolvertest

$(DIR)/$(DEBUG_DIR)iksolvertest:
	@echo "****> linking $@ in $(DIR)"
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)iksolvertest $(LIBS) $(SLIBS) $(LLIBS) $(DADD)

clean::
	$(RM) $(DIR)/iksolvertest $(DIR)/debug/iksolvertest

test:: all
	$(DIR)/iksolvertest

