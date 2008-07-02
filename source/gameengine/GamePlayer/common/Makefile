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

LIBNAME = common
DIR = $(OCGDIR)/gameengine/GamePlayer/$(LIBNAME)

include nan_compile.mk

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += -I$(OPENGL_HEADERS)

CPPFLAGS += -I../../../blender/blenkernel
CPPFLAGS += -I../../../blender/blenloader
CPPFLAGS += -I../../../blender/blenlib
CPPFLAGS += -I../../../blender/imbuf
CPPFLAGS += -I../../../blender/makesdna
CPPFLAGS += -I../../../kernel/gen_system
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

CPPFLAGS += -I$(NAN_FUZZICS)/include
CPPFLAGS += -I$(NAN_STRING)/include    
CPPFLAGS += -I$(NAN_MOTO)/include
CPPFLAGS += -I$(NAN_SUMO)/include
CPPFLAGS += -I$(NAN_SOLID)/include
CPPFLAGS += -I$(NAN_PNG)/include
CPPFLAGS += -I$(NAN_ZLIB)/include
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

CPPFLAGS += -I$(NAN_BMFONT)/include
CPPFLAGS += -I$(NAN_SOUNDSYSTEM)/include
CPPFLAGS += -I../../../gameengine/Converter
CPPFLAGS += -I../../../gameengine/Expressions
CPPFLAGS += -I../../../gameengine/GameLogic
CPPFLAGS += -I../../../gameengine/Converter
CPPFLAGS += -I../../../gameengine/BlenderRoutines
CPPFLAGS += -I../../../gameengine/Ketsji
CPPFLAGS += -I../../../gameengine/Ketsji/KXNetwork
CPPFLAGS += -I../../../gameengine/Network
CPPFLAGS += -I../../../gameengine/Network/LoopBackNetwork
CPPFLAGS += -I../../../gameengine/Rasterizer
CPPFLAGS += -I../../../gameengine/SceneGraph
CPPFLAGS += -I../../../gameengine/Rasterizer/RAS_OpenGLRasterizer
CPPFLAGS += -I../../../gameengine/Physics/Sumo
CPPFLAGS += -I../../../gameengine/Physics/common

ifeq ($(WITH_BF_GLEXT),true)
    CPPFLAGS += -DWITH_GLEXT
endif

###############################

SOURCEDIR = source/gameengine/GamePlayer/common
DIRS =
ifeq ($(OS),windows)
# Still 
    DIRS = windows
else
  ifneq ($(OS),darwin)
    DIRS = unix
  endif
endif

include nan_subdirs.mk
