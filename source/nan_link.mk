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
# linking only

include nan_definitions.mk

ifdef NAN_DEBUG
    LDFLAGS += $(NAN_DEBUG)
endif

DBG_LDFLAGS	+= -g

ifneq (x$(DEBUG_DIR), x)
    LDFLAGS+=$(DBG_LDFLAGS)
else
    LDFLAGS+=$(REL_LDFLAGS)
endif

######################## OS dependencies (alphabetic!) ################

# default (overriden by windows)
SOEXT = .so

ifeq ($(OS),beos)
    LLIBS = -L/boot/develop/lib/x86/ -lGL -lbe -L/boot/home/config/lib/
endif

ifeq ($(OS),darwin)
    LLIBS    += -lGLU -lGL
    LLIBS    += -lz -lstdc++
    ifdef USE_OSX10.4STUBS
       LLIBS    +=-lSystemStubs
    endif 
    LLIBS    += -framework Carbon -framework AGL -framework OpenGL
    LLIBS    += -framework QuickTime -framework CoreAudio
    LLIBS    += -framework AudioUnit -framework AudioToolbox
    LDFLAGS += -L/System/Library/Frameworks/OpenGL.framework/Libraries
    DBG_LDFLAGS += -L/System/Library/Frameworks/OpenGL.framework/Libraries
endif

ifeq ($(OS),freebsd)
    LLIBS = -L/usr/X11R6/lib -lX11 -lXmu -lXi -lm -lutil -lz -pthread -lc_r
    DADD = -lGL -lGLU
    DYNLDFLAGS = -shared $(LDFLAGS)
    LOPTS = -Wl,--export-dynamic
endif

ifeq ($(OS),irix)
    LDFLAGS += -mips3
    LLIBS = -lmovieGL -lGLU -lGL -lXmu -lXext -lX11 -lc -lm -ldmedia
    LLIBS += -lcl -laudio -ldb -lCio -lz
    LLIBS += -lpthread
    LLIBS += -woff 84,171
    DYNLDFLAGS = -shared $(LDFLAGS)
endif

ifeq ($(OS),linux)
  ifeq ($(CPU),alpha)
    COMMENT = "MESA 3.1"
    LLIBS = -lGL -lGLU -L/usr/X11R6/lib/ -lXmu -lXext -lX11
    LLIBS += -lc -lm -ldl -lutil
    LOPTS = -export-dynamic
  endif
  ifeq ($(CPU),$(findstring $(CPU), "i386 x86_64 ia64"))
    COMMENT = "MESA 3.1"
    LLIBS = -L$(NAN_MESA)/lib -L/usr/X11R6/lib -lXmu -lXext -lX11 -lXi
    LLIBS += -lutil -lc -lm -ldl -lpthread 
#    LLIBS += -L$(NAN_ODE)/lib -lode
    LOPTS = -export-dynamic
    DADD = -lGL -lGLU
    SADD = $(NAN_MESA)/lib/libGL.a $(NAN_MESA)/lib/libGLU.a
    DYNLDFLAGS = -shared $(LDFLAGS)
  endif
  ifeq ($(CPU),$(findstring $(CPU), "powerpc sparc64"))
    LLIBS = -L/usr/X11R6/lib/ -lXmu -lXext -lX11 -lc -ldl -lm -lutil
    DADD = -lGL -lGLU
    SADD = /usr/lib/libGL.a /usr/lib/libGLU.a
    LOPTS = -export-dynamic
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
    ifeq ($(CPU), i386)
        LLIBS = -L/usr/X11/lib
    endif 
    LLIBS += -lGLU -lGL -lXmu -lXext -lXi -lX11 -lc -lm -ldl -lsocket -lnsl
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
		ifeq ($(WITH_VERSE),true)
			DADD += -lws2_32
		endif
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
		LOPTS += /NODEFAULTLIB:"python20" 
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

LLIBS += $(NAN_PYTHON_LIB)
