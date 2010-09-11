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
# The Original Code is Copyright (C) 2009 Blender Foundation
# All rights reserved.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****

LIBNAME = minilzo
DIR = $(OCGDIR)/extern/$(LIBNAME)

include nan_compile.mk

install: $(ALL_OR_DEBUG)
	@[ -d $(NAN_LZO) ] || mkdir -p $(NAN_LZO)
	@[ -d $(NAN_LZO)/minilzo ] || mkdir -p $(NAN_LZO)/minilzo
	@[ -d $(NAN_LZO)/lib/$(DEBUG_DIR) ] || mkdir -p $(NAN_LZO)/lib/$(DEBUG_DIR)
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)lib$(LIBNAME).a $(NAN_LZO)/lib/$(DEBUG_DIR)
ifeq ($(OS),darwin)
	ranlib $(NAN_LZO)/lib/$(DEBUG_DIR)lib$(LIBNAME).a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh *.h $(NAN_LZO)/minilzo
