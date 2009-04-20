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

LIBNAME = python
DIR = $(OCGDIR)/blender/python

CSRCS ?= $(wildcard *.c) $(wildcard ../*.c)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

ifdef NAN_BUILDINFO
    CPPFLAGS += -DNAN_BUILDINFO
endif

ifeq ($(WITH_FFMPEG), true)
   CPPFLAGS += -DWITH_FFMPEG
endif

ifeq ($(WITH_OPENEXR),true)
	CPPFLAGS += -DWITH_OPENEXR
endif

CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenloader
CPPFLAGS += -I../../include
CPPFLAGS += -I../../gpu
CPPFLAGS += -I../../render/extern/include
CPPFLAGS += -I../../radiosity/extern/include
CPPFLAGS += -I$(NAN_BMFONT)/include
CPPFLAGS += -I../../imbuf
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

# We'll just add a little duct tape to fix things here ...
.PHONY: clean
clean::
	@rm -f $(OCGDIR)/blender/BPY_*.o
	@rm -f $(OCGDIR)/blender/BPY_*.d
