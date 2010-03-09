# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
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
# Contributor(s): GSR
#
# ***** END GPL LICENSE BLOCK *****
# ghost intern Makefile
#

LIBNAME = ghost
DIR = $(OCGDIR)/intern/$(LIBNAME)

CCSRCS = GHOST_Buttons.cpp GHOST_System.cpp GHOST_Window.cpp
CCSRCS += GHOST_EventManager.cpp GHOST_EventPrinter.cpp GHOST_WindowManager.cpp
CCSRCS += GHOST_ISystem.cpp GHOST_ModifierKeys.cpp GHOST_TimerManager.cpp
CCSRCS += GHOST_Rect.cpp GHOST_DisplayManager.cpp GHOST_C-api.cpp
CCSRCS += GHOST_CallbackEventConsumer.cpp
CCSRCS += GHOST_NDOFManager.cpp

ifeq ($(OS),$(findstring $(OS), "darwin"))
    ifeq ($(WITH_COCOA), true)
        OCCSRCS += $(wildcard *Cocoa.mm)
        CPPFLAGS += -DGHOST_COCOA
        ifeq ($(WITH_QUICKTIME), true)
            CPPFLAGS += -DWITH_QUICKTIME
        endif
    else
        CCSRCS += $(wildcard *Carbon.cpp)
    endif
endif

ifeq ($(OS),$(findstring $(OS), "windows"))
    CPPFLAGS += -I$(NAN_WINTAB)/include
    CCSRCS += $(wildcard *Win32.cpp)
endif

ifeq ($(OS),$(findstring $(OS), "freebsd irix linux openbsd solaris"))
    CCSRCS += $(wildcard *X11.cpp)
endif

include nan_compile.mk

#CCFLAGS += $(LEVEL_2_CPP_WARNINGS)

CPPFLAGS += -I$(NAN_STRING)/include
CPPFLAGS += -I$(NAN_MEMUTIL)/include
CPPFLAGS += -I..
CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I../../../source/blender/imbuf
CPPFLAGS += -I../../../source/blender/makesdna

