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
# The Original Code is Copyright (C) 2002 by Hans Lambermont
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jiri Hnidek
#
# ***** END GPL LICENSE BLOCK *****
LIBNAME = verse
SOURCEDIR = extern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = dist

include nan_subdirs.mk
include nan_compile.mk
include nan_link.mk

DISTDIR = dist
CP = ../../intern/tools/cpifdiff.sh

ifeq ($(OS),windows)
    EXT = .exe
endif

install: all debug
	@[ -d $(NAN_VERSE) ] || mkdir -p $(NAN_VERSE)
	@[ -d $(NAN_VERSE)/include ] || mkdir -p $(NAN_VERSE)/include
	@[ -d $(NAN_VERSE)/lib ] || mkdir -p $(NAN_VERSE)/lib
	@[ -d $(OCGDIR)/bin ] || mkdir -p $(OCGDIR)/bin
	@$(CP) $(DISTDIR)/*.h $(NAN_VERSE)/include
	@$(CP) $(DIR)/libverse.a $(NAN_VERSE)/lib
ifeq ($(OS),darwin)
	ranlib $(NAN_VERSE)/lib/libverse.a
endif
	$(CCC) $(CCFLAGS) $(LDFLAGS) -o $(DIR)/verse$(EXT) $(DIR)/libverse.a $(LIBS) $(SLIBS) $(LLIBS) $(DADD) $(LOPTS)
	@$(CP) $(DIR)/verse$(EXT) $(OCGDIR)/bin
