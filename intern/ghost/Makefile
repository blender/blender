# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
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
# Contributor(s): Hans Lambermont, GSR
#
# ***** END GPL LICENSE BLOCK *****
# ghost main makefile.
#

include nan_definitions.mk

LIBNAME = ghost
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
TESTDIRS = test

include nan_subdirs.mk

install: $(ALL_OR_DEBUG)
	@[ -d $(NAN_GHOST) ] || mkdir $(NAN_GHOST)
	@[ -d $(NAN_GHOST)/include ] || mkdir $(NAN_GHOST)/include
	@[ -d $(NAN_GHOST)/lib/$(DEBUG_DIR) ] || mkdir $(NAN_GHOST)/lib/$(DEBUG_DIR)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libghost.a $(NAN_GHOST)/lib/$(DEBUG_DIR)
ifeq ($(OS),darwin)
	ranlib $(NAN_GHOST)/lib/$(DEBUG_DIR)libghost.a
endif
	@../tools/cpifdiff.sh *.h $(NAN_GHOST)/include/

