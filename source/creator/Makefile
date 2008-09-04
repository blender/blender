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

LIBNAME = creator
DIR = $(OCGDIR)/creator

# this HAS to go
CSRCS = creator.c

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I../blender/render/extern/include
CPPFLAGS += -I../blender/radiosity/extern/include

# two needed for the kernel
CPPFLAGS += -I../blender/imbuf
CPPFLAGS += -I../blender/makesdna
CPPFLAGS += -I../blender/blenlib
CPPFLAGS += -I../blender/include
CPPFLAGS += -I../blender/renderconverter
CPPFLAGS += -I../blender/blenkernel
CPPFLAGS += -I../blender/python
CPPFLAGS += -I../blender/blenloader
CPPFLAGS += -I../blender/gpu
CPPFLAGS += -I../kernel/gen_system
CPPFLAGS += -I../kernel/gen_messaging
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I$(NAN_GLEW)/include

ifeq ($(WITH_QUICKTIME), true)
	CPPFLAGS += -I$(NAN_QUICKTIME)/include -DWITH_QUICKTIME
endif

ifeq ($(WITH_BINRELOC), true)
	CPPFLAGS += -I$(OCGDIR)/extern/binreloc/include -DWITH_BINRELOC
endif

ifeq ($(NAN_YESIAMSTUPID), true)
	CPPFLAGS += -DYESIAMSTUPID
endif

CPPFLAGS += -I$(OPENGL_HEADERS)
