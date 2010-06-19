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

LIBNAME = imbuf
DIR = $(OCGDIR)/blender/imbuf
SOURCEDIR = source/blender/imbuf/intern

include nan_subdirs.mk
include nan_compile.mk
include nan_definitions.mk

DIRS = cineon

ifeq ($(WITH_OPENEXR), true)
    DIRS += openexr
    CFLAGS += -DWITH_OPENEXR
endif

ifeq ($(WITH_DDS), true)
    DIRS += dds
    CPPFLAGS += -DWITH_DDS
endif

ifeq ($(WITH_OPENJPEG), true)
    ifndef BF_OPENJPEG_INC
        CFLAGS += -DWITH_OPENJPEG -I../../../../extern/libopenjpeg
    else
        CFLAGS += -DWITH_OPENJPEG -I$(BF_OPENJPEG_INC)
    endif
endif

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I$(NAN_JPEG)/include
CPPFLAGS += -I$(NAN_PNG)/include
CPPFLAGS += -I$(NAN_ZLIB)/include
CPPFLAGS += -I../../include
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../avi
CPPFLAGS += -I../../quicktime
# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I$(NAN_MEMUTIL)/include
# This is not really needed, but until /include is cleaned, it must be
# there for proper compilation.
# - No, it is also needed in antialias, for listbase (nzc)
CPPFLAGS += -I../../makesdna
# external interface of this module
CPPFLAGS += -I..

ifeq ($(WITH_QUICKTIME), true)
   CPPFLAGS += -DWITH_QUICKTIME
endif

ifeq ($(WITH_FFMPEG), true)
   CPPFLAGS += -DWITH_FFMPEG
   CPPFLAGS += $(NAN_FFMPEGCFLAGS)
endif

ifeq ($(WITH_TIFF), true)
   CPPFLAGS += -DWITH_TIFF
   CPPFLAGS += -I$(NAN_TIFF)/include
endif
