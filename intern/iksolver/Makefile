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

LIBNAME = iksolver
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
TESTDIRS = test

include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_IKSOLVER) ] || mkdir $(NAN_IKSOLVER)
	@[ -d $(NAN_IKSOLVER)/include ] || mkdir $(NAN_IKSOLVER)/include
	@[ -d $(NAN_IKSOLVER)/lib ] || mkdir $(NAN_IKSOLVER)/lib
	@[ -d $(NAN_IKSOLVER)/lib/debug ] || mkdir $(NAN_IKSOLVER)/lib/debug
	@../tools/cpifdiff.sh $(DIR)/libiksolver.a $(NAN_IKSOLVER)/lib/
	@../tools/cpifdiff.sh $(DIR)/debug/libiksolver.a $(NAN_IKSOLVER)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_IKSOLVER)/lib/libiksolver.a
	ranlib $(NAN_IKSOLVER)/lib/debug/libiksolver.a
endif
	@../tools/cpifdiff.sh extern/*.h $(NAN_IKSOLVER)/include/

