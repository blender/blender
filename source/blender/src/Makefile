#
# $Id$
#
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
#

ORGLIBNAME = src
LIBNAME ?= $(ORGLIBNAME)
DIR ?= $(OCGDIR)/blender/$(ORGLIBNAME)

CSRCS ?= $(wildcard *.c)
PUB_CSRCS ?= $(CSRCS) $(wildcard pub/*.c)
CRE_CSRCS ?= $(CSRCS) $(wildcard cre/*.c)

OBJS = $(OCGDIR)/blender/makesdna/$(DEBUG_DIR)DNA.o

ALLTARGETS = creator publisher

include nan_compile.mk

# Cflags stuff ------------------------------------------

CFLAGS += $(LEVEL_1_C_WARNINGS) 
#CFLAGS += $(LEVEL_1_C_WARNINGS) -diag_error 1196

ifeq ($(OS),$(findstring $(OS), "beos darwin freebsd linux openbsd solaris windows"))
    CFLAGS += -funsigned-char
endif

# PreProcessor stuff ------------------------------------------

CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)
CPPFLAGS += -I$(NAN_GHOST)/include
CPPFLAGS += -I$(NAN_BMFONT)/include

# External interfaces of modules:
CPPFLAGS += -I../render/extern/include
CPPFLAGS += -I../renderconverter
CPPFLAGS += -I../blenkernel
CPPFLAGS += -I../blenlib
CPPFLAGS += -I../bpython/include
CPPFLAGS += -I../makesdna
CPPFLAGS += -I../imbuf
CPPFLAGS += -I../blenloader
CPPFLAGS += -I../verify
CPPFLAGS += -I..
CPPFLAGS += -I../misc
CPPFLAGS += -I../../kernel/gen_system
CPPFLAGS += -I../../gameengine/SoundSystem

# Only used by py_demo.c !!!
CPPFLAGS += -I../radiosity/extern/include

CPPFLAGS += -I$(SRCHOME)/sumo/include
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include

CPPFLAGS += -I$(NAN_DECIMATION)/include
CPPFLAGS += -I$(NAN_BSP)/include

CPPFLAGS += -I$(NAN_BLENKEY)/include
CPPFLAGS += -I../readstreamglue
CPPFLAGS += -I../include

ifdef NAN_BUILDINFO
    CPPFLAGS += -DNAN_BUILDINFO
endif

ifeq ($(OS),linux)
    ifeq ($(CPU),alpha)
	CPPFLAGS += -I$(NAN_MESA)/include
    endif
    ifeq ($(CPU),i386)
	CPPFLAGS += -I$(NAN_MESA)/include
    endif
    ifeq ($(CPU),powerpc)
	CPPFLAGS += -I/usr/src/MesaCVS/include
    endif
endif
