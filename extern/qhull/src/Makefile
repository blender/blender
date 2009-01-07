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

LIBNAME = qhull
DIR = $(OCGDIR)/extern/$(LIBNAME)

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += -I../include

CSRCS = user.c global.c stat.c io.c geom2.c poly2.c \
       merge.c qhull.c geom.c poly.c qset.c mem.c
CCSRCS = 
include nan_compile.mk 

install: all debug
	@[ -d $(NAN_QHULL) ] || mkdir -p $(NAN_QHULL)
	@[ -d $(NAN_QHULL)/include/qhull ] || mkdir -p $(NAN_QHULL)/include/qhull
	@[ -d $(NAN_QHULL)/lib ] || mkdir -p $(NAN_QHULL)/lib
	@[ -d $(NAN_QHULL)/lib/debug ] || mkdir -p $(NAN_QHULL)/lib/debug
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/lib$(LIBNAME).a $(NAN_QHULL)/lib/
#	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/debug/lib$(LIBNAME).a $(NAN_QHULL)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_QHULL)/lib/lib$(LIBNAME).a
#	ranlib $(NAN_QHULL)/lib/debug/lib$(LIBNAME).a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh ../include/qhull/*.h $(NAN_QHULL)/include/qhull


