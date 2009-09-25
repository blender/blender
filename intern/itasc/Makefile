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
# Contributor(s): Hans Lambermont
#
# ***** END GPL LICENSE BLOCK *****
# iksolver main makefile.
#

include nan_definitions.mk

LIBNAME = itasc
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = kdl
include nan_subdirs.mk
include nan_compile.mk

CPPFLAGS += -I.
CPPFLAGS += -I../../extern/Eigen2

install: all debug
	@[ -d $(NAN_ITASC) ] || mkdir $(NAN_ITASC)
	@[ -d $(NAN_ITASC)/lib ] || mkdir $(NAN_ITASC)/lib
	@[ -d $(NAN_ITASC)/lib/debug ] || mkdir $(NAN_ITASC)/lib/debug
	@../tools/cpifdiff.sh $(DIR)/libitasc.a $(NAN_ITASC)/lib/
	@../tools/cpifdiff.sh $(DIR)/debug/libitasc.a $(NAN_ITASC)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_ITASC)/lib/libitasc.a
	ranlib $(NAN_ITASC)/lib/debug/libitasc.a
endif
##############################
include nan_subdirs.mk
