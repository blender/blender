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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): GSR
#
# ***** END GPL LICENSE BLOCK *****
#
# linking only

include nan_definitions.mk

ifdef NAN_DEBUG
    LDFLAGS += $(NAN_DEBUG)
endif

DBG_LDFLAGS += -g

ifneq (x$(DEBUG_DIR), x)
    LDFLAGS+=$(DBG_LDFLAGS)
else
    LDFLAGS+=$(REL_LDFLAGS)
endif

######################## OS dependencies (alphabetic!) ################

# default (overriden by windows)
SOEXT = .so

ifeq ($(OS),darwin)
    LLIBS    += -lGLU -lGL
    LLIBS    += -lz -lstdc++
    ifdef USE_OSX10.4STUBS
       LLIBS    +=-lSystemStubs
    endif
    ifeq ($(WITH_COCOA), true)
        LLIBS += -framework Cocoa
    endif
    LLIBS += -framework Carbon -framework AGL -framework OpenGL
    ifeq ($(WITH_QUICKTIME), true)
        ifeq ($(USE_QTKIT), true)
            LLIBS += -framework QTKit
        else
            LLIBS  += -framework QuickTime
        endif
    endif
    LLIBS += -framework CoreAudio
    LLIBS += -framework AudioUnit -framework AudioToolbox
    LDFLAGS += -L/System/Library/Frameworks/OpenGL.framework/Libraries
    # useful for crosscompiling
    LDFLAGS += -arch $(MACOSX_ARCHITECTURE) #-isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MACOSX_MIN_VERS)
    
    DBG_LDFLAGS += -L/System/Library/Frameworks/OpenGL.framework/Libraries
endif

ifeq ($(OS),freebsd)
    LLIBS = -L/usr/X11R6/lib -lX11 -lXmu -lXi -lm -lutil -lz -pthread -lc_r
    DADD = -lGL -lGLU
    DYNLDFLAGS = -shared $(LDFLAGS)
    LOPTS = -Wl,--export-dynamic
endif

ifeq ($(OS),irix)
    ifeq ($(IRIX_USE_GCC), true)
        LDFLAGS += -mabi=n32 -mips4 
        DBG_LDFLAGS += -LD_LAYOUT:lgot_buffer=40
    else
        LDFLAGS += -n32 -mips3
        LDFLAGS += -woff 84,171
    endif
    LLIBS = -lmovieGL -lGLU -lGL -lXmu -lXext -lXi -lX11 -lc -lm -ldmedia
    LLIBS += -lcl -laudio
    ifneq ($(IRIX_USE_GCC), true)
        LLIBS += -lCio -ldb
    endif
    LLIBS += -lz -lpthread
    DYNLDFLAGS = -shared $(LDFLAGS)
endif

ifeq ($(OS),linux)
  ifeq ($(CPU),alpha)
    COMMENT = "MESA 3.1"
    LLIBS = -lGL -lGLU -L/usr/X11R6/lib/ -lXmu -lXext -lX11
    LLIBS += -lc -lm -ldl -lutil
    LOPTS = -export-dynamic
  endif
  ifeq ($(CPU),$(findstring $(CPU), "i386 x86_64 ia64 parisc64 powerpc sparc64"))
    COMMENT = "MESA 3.1"
    LLIBS = -L$(NAN_MESA)/lib -L/usr/X11R6/lib -lXmu -lXext -lX11 -lXi
    LLIBS += -lutil -lc -lm -ldl -lpthread
    LLIBS += -L$(NAN_PYTHON)/lib -Wl,-rpath -Wl,$(NAN_PYTHON)/lib -lpython$(NAN_PYTHON_VERSION)
    LOPTS = -export-dynamic
    DADD = -lGL -lGLU
    SADD = $(NAN_MESA)/lib/libGL.a $(NAN_MESA)/lib/libGLU.a
    DYNLDFLAGS = -shared $(LDFLAGS)
  endif
    LLIBS += -lz
endif

ifeq ($(OS),openbsd)
    SADD = /usr/local/lib/libGL.a /usr/local/lib/libGLU.a
    SADD += /usr/X11R6/lib/libXmu.a /usr/X11R6/lib/libXext.a
    SADD += /usr/X11R6/lib/libX11.a /usr/lib/libm.a -pthread
endif

ifeq ($(OS),solaris)
    ifeq (x86_64, $(findstring x86_64, $(CPU)))
        LLIBS = -lrt
        LLIBS += -L$(NAN_MESA)/lib/amd64
    else
        LLIBS += -L$(NAN_MESA)/lib
    endif
    
    LLIBS += $(NAN_ZLIB)/lib/libz.a -lGLU -lGL -lXmu -lXext -lXi -lX11 -lc -lm -ldl -lsocket -lnsl 
    DYNLDFLAGS = -shared $(LDFLAGS)
endif

ifeq ($(OS),windows)
    EXT = .exe
    SOEXT = .dll
    ifeq ($(FREE_WINDOWS),true)
        MINGWLIB = /usr/lib/w32api
        LDFLAGS += -mwindows -mno-cygwin -mconsole
        DADD += -L/usr/lib/w32api -lnetapi32 -lopengl32 -lglu32 -lshfolder
        DADD += -L/usr/lib/w32api -lwinmm -lwsock32
    else
        DADD = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
        DADD += advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
        DADD += vfw32.lib winmm.lib opengl32.lib glu32.lib largeint.lib dxguid.lib
        DADD += libcmt.lib
        LOPTS = /link
        LOPTS += /NODEFAULTLIB:"libc" 
        LOPTS += /NODEFAULTLIB:"libcd" 
        LOPTS += /NODEFAULTLIB:"libcp" 
        LOPTS += /NODEFAULTLIB:"libcpd" 
        LOPTS += /NODEFAULTLIB:"python31" 
        LOPTS += /NODEFAULTLIB:"msvcrt" 
        LOPTS += /SUBSYSTEM:CONSOLE
        LDFLAGS += /MT
        DYNLDFLAGS = /LD
    endif
endif

ifneq ($(OS), irix)
   LLIBS += $(NAN_SDLLIBS)
endif

ifeq ($(WITH_ICONV),true)
   LLIBS += $(NAN_ICONV_LIBS)
endif

ifeq ($(WITH_FFMPEG),true)
   LLIBS += $(NAN_FFMPEGLIBS)
endif

ifeq ($(INTERNATIONAL),true)
   LLIBS += $(NAN_GETTEXT_LIB)
endif

ifeq ($(WITH_BF_OPENMP),true)
   LLIBS += -lgomp
endif

ifeq ($(WITH_FFTW3),true)
    LLIBS += $(BF_FFTW3_LIBS)
endif

ifeq ($(WITH_OPENCOLLADA),true)
    LLIBS += $(BF_OPENCOLLADA_LIBS)
endif

LLIBS += $(NAN_PYTHON_LIB)
