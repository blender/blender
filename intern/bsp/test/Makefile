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
# bsp test makefile.
#

LIBNAME = bsp
SOURCEDIR = intern/$(LIBNAME)/test
DIR = $(OCGDIR)/$(SOURCEDIR)

include nan_compile.mk

DIRS = BSP_GhostTest

include nan_subdirs.mk

include nan_link.mk

LIBS = $(OCGDIR)/intern/$(LIBNAME)/test/BSP_GhostTest/$(DEBUG_DIR)libBSP_GhostTest.a
LIBS += $(OCGDIR)/intern/$(LIBNAME)/$(DEBUG_DIR)libbsp.a

SLIBS += $(NAN_MOTO)/lib/$(DEBUG_DIR)libmoto.a
SLIBS += $(NAN_GHOST)/lib/$(DEBUG_DIR)libghost.a
SLIBS += $(NAN_STRING)/lib/$(DEBUG_DIR)libstring.a

ifeq ($(OS),$(findstring $(OS), "darwin linux freebsd openbsd"))
    LLIBS = -L/usr/X11R6/lib -lglut -pthread -lXi -lXmu
endif

all debug:: $(LIBS) $(DIR)/$(DEBUG_DIR)BSPGhostTest

$(DIR)/$(DEBUG_DIR)BSPGhostTest:
	@echo "****> linking $@ in $(DIR)" 
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)BSPGhostTest $(LIBS) $(SLIBS) $(LLIBS) $(DADD)

clean::
	$(RM) $(DIR)/BSPGhostTest $(DIR)/debug/BSPGhostTest

test:: all
	$(DIR)/BSPGhostTest





