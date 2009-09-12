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

LIBNAME = videotex
DIR = $(OCGDIR)/gameengine/$(LIBNAME)
SOURCEDIR = source/gameengine/VideoTexture

include nan_compile.mk

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += $(OGL_CPPFLAGS)
CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION) 
CPPFLAGS += -I../../blender/python
CPPFLAGS += -I$(NAN_STRING)/include    
CPPFLAGS += -I$(NAN_MOTO)/include
CPPFLAGS += -I../Rasterizer/RAS_OpenGLRasterizer
CPPFLAGS += -I../Rasterizer -I../GameLogic -I../SceneGraph
CPPFLAGS += -I../BlenderRoutines -I../Expressions -I../Ketsji
CPPFLAGS += -I../../kernel/gen_system
CPPFLAGS += -I.
CPPFLAGS += -I../../blender/blenkernel
CPPFLAGS += -I../../blender/blenlib
CPPFLAGS += -I../../blender/editors/include
CPPFLAGS += -I../../blender/makesdna
CPPFLAGS += -I../../blender/imbuf
CPPFLAGS += -I../../blender/gpu
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

ifeq ($(WITH_FFMPEG),true)
    CPPFLAGS += -DWITH_FFMPEG
    CPPFLAGS += $(NAN_FFMPEGCFLAGS)
	ifdef NAN_PTHREADS
		CPPFLAGS += -I$(NAN_PTHREADS)/include
	endif
endif


