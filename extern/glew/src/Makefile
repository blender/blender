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

LIBNAME = glew
DIR = $(OCGDIR)/extern/$(LIBNAME)

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += -I../include

CSRCS = glew.c
CCSRCS = 
include nan_compile.mk 

install: all debug
	@[ -d $(NAN_GLEW) ] || mkdir -p $(NAN_GLEW)
	@[ -d $(NAN_GLEW)/include/GL ] || mkdir -p $(NAN_GLEW)/include/GL
	@[ -d $(NAN_GLEW)/lib ] || mkdir -p $(NAN_GLEW)/lib
	@[ -d $(NAN_GLEW)/lib/debug ] || mkdir -p $(NAN_GLEW)/lib/debug
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh $(DIR)/lib$(LIBNAME).a $(NAN_GLEW)/lib/
ifeq ($(OS),darwin)
	ranlib $(NAN_GLEW)/lib/lib$(LIBNAME).a
	ranlib $(NAN_GLEW)/lib/lib$(LIBNAME).a
endif
	@$(NANBLENDERHOME)/intern/tools/cpifdiff.sh ../include/GL/*.h $(NAN_GLEW)/include/GL

