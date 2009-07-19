# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
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
# The Original Code is Copyright (C) 2008 by The Blender Foundation
# All rights reserved.
#
#

LIBNAME = binreloc
DIR = $(OCGDIR)/extern/$(LIBNAME)

include nan_definitions.mk

CPPFLAGS += -DENABLE_BINRELOC -I./include


include nan_compile.mk

install: $(ALL_OR_DEBUG)
	@[ -d $(DIR) ] || mkdir $(DIR)
	@[ -d $(DIR)/include ] || mkdir $(DIR)/include
	@../../intern/tools/cpifdiff.sh include/*.h $(DIR)/include/
