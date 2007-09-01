# $Id$
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
# The Original Code is Copyright (C) 2002 by Stichting Blender Foundation,
# Amsterdam, the Netherlands.
# All rights reserved.
#
# The Original Code is: revision 1.1
#
# Contributor(s): Wouter van Heyst
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
# po Makefile for blender. Compiles the translations and places them
# where release can pick them up.

SOURCEDIR = blender/po

include nan_definitions.mk

ifeq ($(INTERNTIONAL),true)
	LINGUAS = ar bg ca cs de es fr hr it ja nl pl pt_BR ro ru sr sr@Latn sv uk zh_CN
else
	LINGUAS = 
endif

ifeq ($(OS), darwin)
DIR = $(OCGDIR)/bin/blender.app/Contents/Resources/locale/$@/LC_MESSAGES/
else
DIR = $(OCGDIR)/bin/.blender/locale/$@/LC_MESSAGES/
endif

all debug:: $(LINGUAS)

clean::
ifeq ($(OS), darwin)
	rm -rf $(OCGDIR)/bin/blender.app/Contents/Resources/locale/
else
	rm -rf $(OCGDIR)/bin/.blender/locale/
endif

$(LINGUAS):
	mkdir -p $(DIR)
	msgfmt -o $(DIR)/blender.mo $@.po
