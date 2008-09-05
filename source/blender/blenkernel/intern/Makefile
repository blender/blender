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
# Reference to the types in makesdna and imbuf
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../imbuf
# This mod uses the BLI and BLO module
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenloader
CPPFLAGS += -I../../python
# also avi is used
CPPFLAGS += -I../../avi
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

# we still refer to /include a bit...
CPPFLAGS += -I../../include

# to include the render stuff:
CPPFLAGS += -I../../render/extern/include

# for image stamping
CPPFLAGS += -I$(NAN_BMFONT)/include
# for sound
#CPPFLAGS += -I../../../kernel/gen_system
CPPFLAGS +=  $(NAN_SDLCFLAGS)

CPPFLAGS += -I$(NAN_IKSOLVER)/include
CPPFLAGS += -I$(NAN_DECIMATION)/include
CPPFLAGS += -I$(NAN_ELBEEM)/include
CPPFLAGS += -I$(NAN_OPENNL)/include

# path to zlib
CPPFLAGS += -I$(NAN_ZLIB)/include

#path to nodes
CPPFLAGS += -I../../nodes

#path to gpu
CPPFLAGS += -I../../gpu

# path to our own external headerfiles
CPPFLAGS += -I..

# path to bullet2, for cloth
CPPFLAGS += -I../../../../extern/bullet2/src

ifeq ($(WITH_FREETYPE2), true)
    CPPFLAGS += -DWITH_FREETYPE2
    CPPFLAGS += -I$(NAN_FREETYPE)/include
    CPPFLAGS += -I$(NAN_FREETYPE)/include/freetype2
endif

ifeq ($(WITH_VERSE), true)
    CPPFLAGS += -DWITH_VERSE
    CPPFLAGS += -I$(NAN_VERSE)/include
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

ifeq ($(WITH_QUICKTIME), true)
	CPPFLAGS += -I../../quicktime
	CPPFLAGS += -DWITH_QUICKTIME
endif
