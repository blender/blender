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

ORGLIBNAME = src
LIBNAME ?= $(ORGLIBNAME)
DIR ?= $(OCGDIR)/blender/$(ORGLIBNAME)

CSRCS ?= $(wildcard *.c)
PUB_CSRCS ?= $(CSRCS)
CRE_CSRCS ?= $(CSRCS) $(wildcard cre/*.c)

OBJS = $(OCGDIR)/blender/makesdna/$(DEBUG_DIR)DNA.o

ALLTARGETS = publisher

include nan_compile.mk

# Cflags stuff ------------------------------------------

CFLAGS += $(LEVEL_1_C_WARNINGS) 
#CFLAGS += $(LEVEL_1_C_WARNINGS) -diag_error 1196

# PreProcessor stuff ------------------------------------------

CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)
CPPFLAGS += -I$(NAN_GHOST)/include
CPPFLAGS += -I$(NAN_BMFONT)/include
CPPFLAGS += -I$(NAN_ELBEEM)/include
CPPFLAGS += -I$(NAN_SOUNDSYSTEM)/include $(NAN_SDLCFLAGS)

# External interfaces of modules:
CPPFLAGS += -I../render/extern/include
CPPFLAGS += -I../blenkernel
CPPFLAGS += -I../blenlib
CPPFLAGS += -I../nodes
CPPFLAGS += -I../python
CPPFLAGS += -I../makesdna
CPPFLAGS += -I../imbuf
CPPFLAGS += -I../blenloader
CPPFLAGS += -I..
CPPFLAGS += -I../../kernel/gen_system
CPPFLAGS += -I../../gameengine/SoundSystem
CPPFLAGS += -I../ftfont

# Only used by py_demo.c !!!
CPPFLAGS += -I../radiosity/extern/include

CPPFLAGS += -I$(SRCHOME)/gameengine/Physics/Sumo/include
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I$(NAN_MEMUTIL)/include

CPPFLAGS += -I$(NAN_DECIMATION)/include
CPPFLAGS += -I$(NAN_BSP)/include
CPPFLAGS += -I$(NAN_OPENNL)/include

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

ifeq ($(WITH_QUICKTIME),true)
	CPPFLAGS += -I../quicktime
	CPPFLAGS += -DWITH_QUICKTIME
endif

ifeq ($(WITH_ICONV), true)
	CPPFLAGS += -DWITH_ICONV
	CPPFLAGS += $(NAN_ICONV_INC)
endif

ifeq ($(WITH_FFMPEG),true)
        CPPFLAGS += -DWITH_FFMPEG
        CPPFLAGS += $(NAN_FFMPEGCFLAGS)
endif

ifdef NAN_PTHREADS
	CPPFLAGS += -I$(NAN_PTHREADS)/include
endif

ifeq ($(WITH_OPENEXR),true)
	CPPFLAGS += -DWITH_OPENEXR
endif

ifeq ($(WITH_DDS),true)
	CPPFLAGS += -DWITH_DDS
endif

ifeq ($(INTERNATIONAL), true)
    CPPFLAGS += -DINTERNATIONAL
endif

ifeq ($(WITH_VERSE), true)
   CPPFLAGS += -DWITH_VERSE
   CPPFLAGS += -I$(NAN_VERSE)/include
   # print some other debug information
   ifeq ($(VERSE_DEBUG_PRINT), true)
       CPPFLAGS += -DVERSE_DEBUG_PRINT
   endif
endif

ifeq ($(NAN_TWEAK_MODE), true)
    CPPFLAGS += -DTWEAK_MODE
endif

CPPFLAGS += -I$(NAN_GLEW)/include
CPPFLAGS += -I$(OPENGL_HEADERS)
