#
# $Id: Makefile,v 1.1.1.1 2002/10/12 11:37:43 hans Exp $
#
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#

include nan_definitions.mk

DIR = $(OCGDIR)/$(DEBUG_DIR)

all:: 
	@# set up directory structure for the OSX aplication bundle
	@echo "---> creating directory structure for $(APPLICATION)"
	@rm -rf $(DIR)/$(APPLICATION).app
	@cp -R $(APPLICATION).app $(DIR)
	@#cat $(APPLICATION).app/Contents/Info.plist | sed s/VERSION/`cat ../../VERSION`/ | sed s/DATE/`date +'%Y-%b-%d'`/ > $(DIR)/$(APPLICATION).app/Contents/Info.plist
	@echo "---> copying binary"
	@cp $(DIR)/$(APPLICATION) $(DIR)/$(APPLICATION).app/Contents/MacOS/
	@echo "---> removing CVS directories and Mac hidden files from distribution"
	@find $(DIR)/$(APPLICATION).app -name CVS -prune -exec rm -rf {} \;
	@find $(DIR)/$(APPLICATION).app -name .DS_Store -exec rm -f {} \;
