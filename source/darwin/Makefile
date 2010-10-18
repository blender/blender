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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****
#

include nan_definitions.mk

DIR = $(OCGDIR)/$(DEBUG_DIR)
VERSION = $(shell $(NANBLENDERHOME)/release/getversion.py)

PYARCHIVE = python_$(MACOSX_ARCHITECTURE).zip

all::
	@# set up directory structure for the OSX aplication bundle
	@echo "---> creating directory structure for $(APPLICATION)"
	@rm -rf $(DIR)/bin/$(APPLICATION).app
	@cp -R $(APPLICATION).app $(DIR)/bin
	@cat $(APPLICATION).app/Contents/Info.plist | sed s/VERSION/$(VERSION)/ | sed s/DATE/`date +'%Y-%b-%d'`/ > $(DIR)/bin/$(APPLICATION).app/Contents/Info.plist
	@echo "---> copying binary"
	@cp $(DIR)/bin/$(APPLICATION) $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/
	@echo "---> adding excutable attributes"
	@chmod +x $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/$(APPLICATION)
ifeq ($(APPLICATION), blender)
	@mkdir -p $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/$(VERSION)
	@echo "---> copying message files"
	@cp -R $(NANBLENDERHOME)/release/bin/.blender/locale $(DIR)/bin/$(APPLICATION).app/Contents/Resources
	@echo "---> copying .Blanguages"
	@cp $(NANBLENDERHOME)/release/bin/.blender/.Blanguages $(DIR)/bin/$(APPLICATION).app/Contents/Resources
	@echo "---> copying bfont.ttf"
	@cp $(NANBLENDERHOME)/release/datafiles/bfont.ttf $(DIR)/bin/$(APPLICATION).app/Contents/Resources/
	@cp $(NANBLENDERHOME)/release/datafiles/bmonofont.ttf $(DIR)/bin/$(APPLICATION).app/Contents/Resources/
	@echo "---> copying release scripts"
	@cp -R $(NANBLENDERHOME)/release/scripts $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/$(VERSION)/
	@echo "---> copying python modules"
	@mkdir $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/$(VERSION)/python
	@unzip -q $(LCGDIR)/release/$(PYARCHIVE) -d $(DIR)/bin/$(APPLICATION).app/Contents/MacOS/$(VERSION)/python/	
endif
	@echo "---> removing SVN directories and Mac hidden files from distribution"
	@find $(DIR)/bin/$(APPLICATION).app -name CVS -prune -exec rm -rf {} \;
	@find $(DIR)/bin/$(APPLICATION).app -name .DS_Store -exec rm -f {} \;
	@find $(DIR)/bin/$(APPLICATION).app -name .svn -prune -exec rm -rf {} \;
