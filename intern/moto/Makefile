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
# moto main makefile.
#

include nan_definitions.mk

LIBNAME = moto
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
#not ready yet TESTDIRS = test
 
include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_MOTO) ] || mkdir $(NAN_MOTO)
	@[ -d $(NAN_MOTO)/include ] || mkdir $(NAN_MOTO)/include
	@[ -d $(NAN_MOTO)/lib ] || mkdir $(NAN_MOTO)/lib
	@[ -d $(NAN_MOTO)/lib/debug ] || mkdir $(NAN_MOTO)/lib/debug
	cp -f $(DIR)/libmoto.a $(NAN_MOTO)/lib/
	cp -f $(DIR)/debug/libmoto.a $(NAN_MOTO)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_MOTO)/lib/libmoto.a
	ranlib $(NAN_MOTO)/lib/debug/libmoto.a
endif
	cp -f include/*.h $(NAN_MOTO)/include/

