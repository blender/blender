#
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
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
#

LIBNAME = solid
DIR = $(OCGDIR)/extern/$(LIBNAME)
DIRS = broad complex convex

include nan_subdirs.mk

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += -I../include -I$(NAN_QHULL)/include
CPPFLAGS += -Iconvex -Icomplex
CPPFLAGS += -DQHULL -DUSE_DOUBLES

include nan_compile.mk 

install: all debug
	@[ -d $(NAN_SOLID) ] || mkdir -p $(NAN_SOLID)
	@[ -d $(NAN_SOLID)/include/SOLID ] || mkdir -p $(NAN_SOLID)/include/SOLID
	@[ -d $(NAN_SOLID)/include/SOLID/MT ] || mkdir -p $(NAN_SOLID)/include/SOLID/MT
	@[ -d $(NAN_SOLID)/lib ] || mkdir -p $(NAN_SOLID)/lib
	@[ -d $(NAN_SOLID)/lib/debug ] || mkdir -p $(NAN_SOLID)/lib/debug
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/lib$(LIBNAME).a $(NAN_SOLID)/lib/
#	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/debug/lib$(LIBNAME).a $(NAN_SOLID)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_SOLID)/lib/lib$(LIBNAME).a
	ranlib $(NAN_SOLID)/lib/debug/lib$(LIBNAME).a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh ../include/*.h $(NAN_SOLID)/include/SOLID
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh ../include/MT/*.h $(NAN_SOLID)/include/SOLID/MT
	@for i in $(DIRS); do \
	    echo "====> $(MAKE) $@ in $(SOURCEDIR)/$$i" ;\
	    $(MAKE) -C $$i install || exit 1; \
	done


