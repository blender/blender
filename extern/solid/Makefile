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
#
#

include nan_definitions.mk

SOURCEDIR = extern/solid
LIBNAMES = solid solid_broad solid_convex solid_complex
DIR = $(OCGDIR)/extern/
DIRS = src

include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_SOLID) ] || mkdir -p $(NAN_SOLID)
	@[ -d $(NAN_SOLID)/include/SOLID ] || mkdir -p $(NAN_SOLID)/include/SOLID
	@[ -d $(NAN_SOLID)/include/SOLID/MT ] || mkdir -p $(NAN_SOLID)/include/SOLID/MT
	@[ -d $(NAN_SOLID)/lib ] || mkdir -p $(NAN_SOLID)/lib
	@[ -d $(NAN_SOLID)/lib/debug ] || mkdir -p $(NAN_SOLID)/lib/debug
	@for i in $(LIBNAMES); do \
	    $(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/$$i/lib$$i.a $(NAN_SOLID)/lib/ ; \
	    $(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/$$i/debug/lib$$i.a $(NAN_SOLID)/lib/debug/ ; \
	    if [ $(OS) = darwin ] ; then \
            ranlib $(NAN_SOLID)/lib/lib$$i.a ; \
            ranlib $(NAN_SOLID)/lib/debug/lib$$i.a ; \
        fi ; \
	done
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh include/*.h $(NAN_SOLID)/include/SOLID
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh include/MT/*.h $(NAN_SOLID)/include/SOLID/MT


