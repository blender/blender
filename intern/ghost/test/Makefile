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
# GHOST test applications makefile.
# This bounces to test application directories.
#

LIBNAME = ghost
SOURCEDIR = intern/$(LIBNAME)/test
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = gears multitest

include nan_subdirs.mk

include nan_compile.mk
include nan_link.mk

OCGGHOST = $(OCGDIR)/intern/$(LIBNAME)
GEARDIR = $(OCGGHOST)/test/$(DEBUG_DIR)gears.app

LIBS = $(OCGGHOST)/$(DEBUG_DIR)libghost.a
SLIBS += $(LCGDIR)/string/lib/libstring.a

all debug:: $(LIBS)
	@echo "****> linking $@ in $(SOURCEDIR)"
ifeq ($(OS),darwin)
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)gears $(DIR)/$(DEBUG_DIR)GHOST_Test.o $(LIBS) $(SLIBS) $(LLIBS) $(DADD) $(LOPTS)
	@# set up directory structure for the OSX application bundle
	@[ -d $(OCGGHOST)/test/ ] || mkdir $(OCGGHOST)/test/
	@[ -d $(OCGGHOST)/test/debug ] || mkdir $(OCGGHOST)/test/debug
	@[ -d $(GEARDIR) ] || mkdir $(GEARDIR)
	@[ -d $(GEARDIR)/Contents ] || mkdir $(GEARDIR)/Contents
	@[ -d $(GEARDIR)/Contents/MacOS ] || mkdir $(GEARDIR)/Contents/MacOS
	@[ -d $(GEARDIR)/Contents/Resources ] || mkdir $(GEARDIR)/Contents/Resources
	@[ -d $(GEARDIR)/Contents/Resources/English.lproj ] || mkdir $(GEARDIR)/Contents/Resources/English.lproj
	@[ -d $(GEARDIR)/Contents/Resources/English.lproj/MainMenu.nib ] || mkdir $(GEARDIR)/Contents/Resources/English.lproj/MainMenu.nib
	@# copy the files into the bundle directory tree
	cp -f $(DIR)/$(DEBUG_DIR)gears $(GEARDIR)/Contents/MacOS
	cp -f gears/resources/osx/PkgInfo $(GEARDIR)/Contents/
	cp -f gears/resources/osx/Info.plist $(GEARDIR)/Contents/
	cp -f gears/resources/osx/English.lproj/InfoPlist.strings $(GEARDIR)/Contents/Resources/English.lproj
	cp -f gears/resources/osx/English.lproj/MainMenu.nib/classes.nib $(GEARDIR)/Contents/Resources/English.lproj
	cp -f gears/resources/osx/English.lproj/MainMenu.nib/info.nib $(GEARDIR)/Contents/Resources/English.lproj
	cp -f gears/resources/osx/English.lproj/MainMenu.nib/objects.nib $(GEARDIR)/Contents/Resources/English.lproj
else
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)gears_cpp $(DIR)/$(DEBUG_DIR)GHOST_Test.o $(LIBS) $(SLIBS) $(LLIBS) $(DADD) $(LOPTS)
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)gears_c $(DIR)/$(DEBUG_DIR)GHOST_C-Test.o $(LIBS) $(SLIBS) $(LLIBS) $(DADD) $(LOPTS)
endif

clean::
	@# mac stuff. well ok, only the binary
	@rm -f $(DIR)/gears     $(DIR)/debug/gears
	@# others
	@rm -f $(DIR)/gears_c   $(DIR)/debug/gears_c
	@rm -f $(DIR)/gears_cpp $(DIR)/debug/gears_cpp

test:: all
	$(DIR)/gears_cpp
	$(DIR)/gears_c
