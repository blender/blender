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
# The Original Code is Copyright (C) 2008 Blender Foundation.
# All rights reserved.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****
#
#

LIBNAME = blenfont
DIR = $(OCGDIR)/blender/blenfont

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# OpenGL and Freetype2
CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += $(OGL_CPPFLAGS)
CPPFLAGS += -I$(NAN_FREETYPE)/include

ifeq ($(OS), windows)
  CPPFLAGS += -I$(NAN_ICONV)/include
  ifeq ($(FREE_WINDOWS), true)
    CPPFLAGS += -I$(NAN_FREETYPE)/include/freetype2
    CPPFLAGS += -DUSE_GETTEXT_DLL
  endif
else
  CPPFLAGS += -I$(NAN_FREETYPE)/include/freetype2
endif

ifeq ($(OS),linux)
    ifeq ($(CPU),alpha)
	CPPFLAGS += -I$(NAN_MESA)/include
    endif
    ifeq ($(CPU),i386)
	CPPFLAGS += -I$(NAN_MESA)/include
    endif
    ifeq ($(CPU),powerpc)
	CPPFLAGS += -I/usr/src/MesaCVS/include
    endif
endif

# Modules
CPPFLAGS += -I../../editors/include
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel

# Memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

# Our own headers
CPPFLAGS += -I..
