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
# ATTENTION: this Makefile is only used for win builds!

LIBNAME = windows
DIR = $(OCGDIR)/gameengine/GamePlayer/common/$(LIBNAME)

include nan_compile.mk

CPPFLAGS += -I..
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

CPPFLAGS += -I$(SRCHOME)/gameengine/GameLogic
CPPFLAGS += -I$(SRCHOME)/gameengine/Rasterizer
CPPFLAGS += -I$(SRCHOME)/gameengine/Ketsji
CPPFLAGS += -I$(SRCHOME)/gameengine/Network
CPPFLAGS += -I$(SRCHOME)/gameengine/Network/LoopBackNetwork

CPPFLAGS += -I$(SRCHOME)/gameengine/Physics/Sumo/Fuzzics/include
CPPFLAGS += -I$(SRCHOME)/gameengine/Physics/Sumo/include

CPPFLAGS += -I$(NAN_MOTO)/include
CPPFLAGS += -I$(NAN_STRING)/include    
CPPFLAGS += -I$(NAN_GLEW)/include

# Blender stuff
CPPFLAGS += -I$(SRCHOME)/blender/blenkernel
CPPFLAGS += -I$(SRCHOME)/blender/blenlib
CPPFLAGS += -I$(SRCHOME)/blender/blenloader
CPPFLAGS += -I$(SRCHOME)/blender/imbuf
CPPFLAGS += -I$(SRCHOME)/blender/makesdna
CPPFLAGS += -I$(SRCHOME)/kernel/gen_system

CPPFLAGS += -I../../kernel/gen_system

