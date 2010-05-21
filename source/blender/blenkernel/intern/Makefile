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

LIBNAME = blenkernel
DIR = $(OCGDIR)/blender/$(LIBNAME)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# OpenGL and Python
CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../../../intern/memutil
CPPFLAGS += -I$(NAN_AUDASPACE)/include
# Reference to the types in makesdna and imbuf
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../makesrna
CPPFLAGS += -I../../imbuf
CPPFLAGS += -I../../ikplugin
# This mod uses the BLI and BLO module
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenloader
CPPFLAGS += -I../../python
CPPFLAGS += -I../../blenfont
# This is bad level, remove eventually
CPPFLAGS += -I../../windowmanager
# also avi is used
CPPFLAGS += -I../../avi
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

# we still refer to /include a bit...
CPPFLAGS += -I../../editors/include

# to include the render stuff:
CPPFLAGS += -I../../render/extern/include

# for sound
#CPPFLAGS += -I../../../kernel/gen_system
CPPFLAGS +=  $(NAN_SDLCFLAGS)

CPPFLAGS += -I$(NAN_IKSOLVER)/include
CPPFLAGS += -I$(NAN_DECIMATION)/include
CPPFLAGS += -I$(NAN_ELBEEM)/include
CPPFLAGS += -I$(NAN_OPENNL)/include
CPPFLAGS += -I$(NAN_BSP)/include
CPPFLAGS += -I$(NAN_SMOKE)/include

# path to zlib
CPPFLAGS += -I$(NAN_ZLIB)/include

#path to nodes
CPPFLAGS += -I../../nodes

#path to gpu
CPPFLAGS += -I../../gpu

#modifiers got moved
CPPFLAGS += -I../../modifiers

# path to our own external headerfiles
CPPFLAGS += -I..

CPPFLAGS += -I$(NAN_FREETYPE)/include
CPPFLAGS += -I$(NAN_FREETYPE)/include/freetype2

# path to bullet2, for cloth
ifeq ($(NAN_USE_BULLET), true)
	CPPFLAGS += -I$(NAN_BULLET2)/include
endif

# lzo and lzma, for pointcache
ifeq ($(WITH_LZO),true)
	CPPFLAGS += -I$(NAN_LZO)/minilzo
	CPPFLAGS += -DWITH_LZO
endif

ifeq ($(WITH_LZO),true)
	CPPFLAGS += -I$(NAN_LZMA)
	CPPFLAGS += -DWITH_LZMA
endif

ifeq ($(WITH_FFMPEG),true)
    CPPFLAGS += -DWITH_FFMPEG
    CPPFLAGS += $(NAN_FFMPEGCFLAGS)
endif

ifeq ($(WITH_OPENEXR), true)
    CPPFLAGS += -DWITH_OPENEXR
endif

ifeq ($(WITH_DDS), true)
    CPPFLAGS += -DWITH_DDS
endif

ifeq ($(WITH_OPENJPEG), true)
    CPPFLAGS += -DWITH_OPENJPEG
endif

ifeq ($(WITH_QUICKTIME), true)
	CPPFLAGS += -I../../quicktime
	CPPFLAGS += -DWITH_QUICKTIME
endif

ifeq ($(WITH_TIFF), true)
    CPPFLAGS += -DWITH_TIFF
endif

ifeq ($(OS), darwin)
    ifeq ($(WITH_BF_OPENMP), true)
      CPPFLAGS += -DPARALLEL=1
    endif
endif
