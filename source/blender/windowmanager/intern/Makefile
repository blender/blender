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

LIBNAME = windowmanager
DIR = $(OCGDIR)/blender/$(LIBNAME)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# OpenGL and Python
CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += $(OGL_CPPFLAGS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

# PreProcessor stuff 

CPPFLAGS += -I$(NAN_GHOST)/include
CPPFLAGS += -I$(NAN_ELBEEM)/include
CPPFLAGS += $(NAN_SDLCFLAGS)

# modules
CPPFLAGS += -I../../editors/include
CPPFLAGS += -I../../python
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../makesrna
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../nodes
CPPFLAGS += -I../../imbuf
CPPFLAGS += -I../../blenloader
CPPFLAGS += -I../../gpu
CPPFLAGS += -I../../render/extern/include
CPPFLAGS += -I../../../kernel/gen_system
CPPFLAGS += -I../../blenfont

# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I$(NAN_MEMUTIL)/include

ifeq ($(INTERNATIONAL), true)
    CPPFLAGS += -DINTERNATIONAL
endif

ifeq ($(WITH_QUICKTIME),true)
	CPPFLAGS += -I../quicktime
	CPPFLAGS += -DWITH_QUICKTIME
endif

ifeq ($(WITH_OPENCOLLADA), true)
    CPPFLAGS += -DWITH_COLLADA
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

# path to our own headerfiles
CPPFLAGS += -I..
