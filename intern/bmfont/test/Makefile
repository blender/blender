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
# Test the bmfont module
#

LIBNAME = bmfont
SOURCEDIR = intern/$(LIBNAME)/test
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = simpletest

include nan_subdirs.mk
include nan_compile.mk
include nan_link.mk

TESTLIBS  = $(OCGDIR)/intern/$(LIBNAME)/$(DEBUG_DIR)lib$(LIBNAME).a
TESTLIBS += $(NAN_STRING)/lib/$(DEBUG_DIR)libstring.a
TESTLIBS +=  $(LCGDIR)/ghost/$(DEBUG_DIR)lib/libghost.a


ifeq ($(OS),$(findstring $(OS), "beos darwin linux freebsd openbsd"))
    TESTLIBS += -L/usr/X11R6/lib -lglut -pthread
endif

ifeq ($(OS),$(findstring $(OS), "solaris"))
    TESTLIBS += -L/usr/openwin/lib -lglut -lX11 -lGL -lGLU -lXmu
endif


all debug::
	@echo "****> linking $@ in $(SOURCEDIR)"
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)BMF_Test $(DIR)/BMF_Test.o $(TESTLIBS)

clean::
	$(RM) $(DIR)/BMF_Test $(DIR)/debug/BMF_Test

test:: $(DIR)/BMF_Test
	$(DIR)/BMF_Test $(NAN_TEST_VERBOSITY)

