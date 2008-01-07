#
# $Id: Makefile 11904 2007-08-31 16:16:33Z sirdude $
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
DIR = $(OCGDIR)/blender/$(LIBNAME)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# OpenGL and Python
CPPFLAGS += $(OGL_CPPFLAGS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

# PreProcessor stuff 

CPPFLAGS += -I$(NAN_GHOST)/include
CPPFLAGS += -I$(NAN_BMFONT)/include
CPPFLAGS += -I$(NAN_SOUNDSYSTEM)/include $(NAN_SDLCFLAGS)

# modules
CPPFLAGS += -I../../editors/include
CPPFLAGS += -I../../python
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../nodes
CPPFLAGS += -I../../imbuf
CPPFLAGS += -I../../blenloader
CPPFLAGS += -I../../render/extern/include

# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I$(NAN_MEMUTIL)/include


ifeq ($(WITH_VERSE), true)
   CPPFLAGS += -DWITH_VERSE
   CPPFLAGS += -I$(NAN_VERSE)/include
   # print some other debug information
   ifeq ($(VERSE_DEBUG_PRINT), true)
       CPPFLAGS += -DVERSE_DEBUG_PRINT
   endif
endif

# path to our own headerfiles
CPPFLAGS += -I..
