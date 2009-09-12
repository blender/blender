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
#
# set some defaults when these are not overruled (?=) by environment variables
#

sinclude ../user-def.mk

# This warning only takes place once in source/
ifeq (debug, $(findstring debug, $(MAKECMDGOALS)))
  ifeq (all, $(findstring all, $(MAKECMDGOALS)))
    export ERRTXT = "ERROR: all and debug targets cannot be used together anymore"
    export ERRTXT += "Use something like ..make all && make debug.. instead"
  endif
endif

ifdef ERRTXT
$(error $(ERRTXT))
endif

ifndef CONFIG_GUESS
  ifeq (debug, $(findstring debug, $(MAKECMDGOALS)))
    export DEBUG_DIR = debug/
    export ALL_OR_DEBUG = debug
  endif
  ifeq (all, $(findstring all, $(MAKECMDGOALS)))
    export ALL_OR_DEBUG ?= all
  endif

  # First generic defaults for all platforms which should be constant.
  # Note: ?= lets these defaults be overruled by environment variables,
    export SRCHOME ?= $(NANBLENDERHOME)/source
    export CONFIG_GUESS := $(shell ${SRCHOME}/tools/guess/guessconfig)
    export OS := $(shell echo ${CONFIG_GUESS} | sed -e 's/-.*//')
    export OS_VERSION := $(shell echo ${CONFIG_GUESS} | sed -e 's/^[^-]*-//' -e 's/-[^-]*//')
    export CPU := $(shell echo ${CONFIG_GUESS} | sed -e 's/^[^-]*-[^-]*-//')
    export MAKE_START := $(shell date "+%H:%M:%S %d-%b-%Y")
    export NAN_LIBDIR ?= $(NANBLENDERHOME)/../lib
    export NAN_OBJDIR ?= $(NANBLENDERHOME)/obj
    # Library Config_Guess DIRectory
    export LCGDIR = $(NAN_LIBDIR)/$(CONFIG_GUESS)
    # Object Config_Guess DIRectory
    export OCGDIR = $(NAN_OBJDIR)/$(CONFIG_GUESS)

    # Determines what targets are built
    export WITH_BF_DYNAMICOPENGL ?= true
    export WITH_BF_STATICOPENGL ?= false
    export WITH_BF_BLENDERGAMEENGINE ?= true
    export WITH_BF_BLENDERPLAYER ?= true
    ifeq ($(NAN_NO_PLUGIN), true)
        export WITH_BF_WEBPLUGIN = false
    else
        export WITH_BF_WEBPLUGIN ?= false
    endif

    export NAN_MOTO ?= $(LCGDIR)/moto
    export BF_PROFILE ?= false
    export NAN_USE_BULLET ?= true
    export NAN_BULLET2 ?= $(LCGDIR)/bullet2
    export NAN_DECIMATION ?= $(LCGDIR)/decimation
    export NAN_GUARDEDALLOC ?= $(LCGDIR)/guardedalloc
    export NAN_IKSOLVER ?= $(LCGDIR)/iksolver
    export NAN_BSP ?= $(LCGDIR)/bsp
    export NAN_BOOLOP ?= $(LCGDIR)/boolop
    export NAN_AUDASPACE ?= $(LCGDIR)/audaspace
    export NAN_STRING ?= $(LCGDIR)/string
    export NAN_MEMUTIL ?= $(LCGDIR)/memutil
    export NAN_CONTAINER ?= $(LCGDIR)/container
    export NAN_ACTION ?= $(LCGDIR)/action
    export NAN_GHOST ?= $(LCGDIR)/ghost
    export NAN_TEST_VERBOSITY ?= 1
    export NAN_OPENNL ?= $(LCGDIR)/opennl
    export NAN_ELBEEM ?= $(LCGDIR)/elbeem
    export NAN_SMOKE ?= $(LCGDIR)/smoke
    export NAN_SUPERLU ?= $(LCGDIR)/superlu
    export NAN_GLEW ?= $(LCGDIR)/glew
    ifeq ($(FREE_WINDOWS), true)
      export NAN_FFMPEG ?= $(LCGDIR)/gcc/ffmpeg
      export NAN_FFMPEGLIBS ?= $(NAN_FFMPEG)/lib/libavformat.a $(NAN_FFMPEG)/lib/libavutil.a $(NAN_FFMPEG)/lib/libavcodec.a $(NAN_FFMPEG)/lib/libavdevice.a
    else
      export NAN_FFMPEG ?= $(LCGDIR)/ffmpeg
      export NAN_FFMPEGLIBS ?= $(NAN_FFMPEG)/lib/libavformat.a $(NAN_FFMPEG)/lib/libavcodec.a $(NAN_FFMPEG)/lib/libswscale.a $(NAN_FFMPEG)/lib/libavutil.a $(NAN_FFMPEG)/lib/libavdevice.a
    endif
    export NAN_FFMPEGCFLAGS ?= -I$(NAN_FFMPEG)/include -I$(NANBLENDERHOME)/extern/ffmpeg

    export WITH_OPENEXR ?= true
    export WITH_DDS ?= true
    export WITH_OPENJPEG ?= true
    export WITH_LZO ?= true
    export WITH_LZMA ?= true
    export NAN_LZO ?= $(LCGDIR)/lzo
    export NAN_LZMA ?= $(LCGDIR)/lzma
    export WITH_OPENAL ?= false
    export WITH_JACK ?= false
    export WITH_SNDFILE ?= false

  ifeq ($(WITH_OPENAL), true)
    export NAN_OPENAL ?= /usr
  endif

  ifeq ($(WITH_JACK), true)
    export NAN_JACK ?= /usr
    export NAN_JACKCFLAGS ?= -I$(NAN_JACK)/include/jack
    export NAN_JACKLIBS ?= $(NAN_JACK)/lib/libjack.a
  endif

  ifeq ($(WITH_SNDFILE),true)
    export NAN_SNDFILE ?= /usr
    export NAN_SNDFILECFLAGS ?= -I$(NAN_SNDFILE)/include
    export NAN_SNDFILELIBS ?= $(NAN_SNDFILE)/lib/libsndfile.a $(NAN_SNDFILE)/lib/libFLAC.a $(NAN_SNDFILE)/lib/libogg.a
  endif

  ifeq ($(NAN_USE_FFMPEG_CONFIG), true)
    export NAN_FFMPEG ?= $(shell ffmpeg-config --prefix)
    export NAN_FFMPEGLIBS ?= $(shell ffmpeg-config --libs avformat avcodec)
    export NAN_FFMPEGCFLAGS ?= $(shell ffmpeg-config --cflags)
  endif

  # Platform Dependent settings go below:
  ifeq ($(OS),darwin)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)

    export NAN_PYTHON_VERSION = 3.1

    ifeq ($(NAN_PYTHON_VERSION),3.1)
      export PY_FRAMEWORK ?= 0
      export NAN_PYTHON ?= $(LCGDIR)/python
      export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/libpython$(NAN_PYTHON_VERSION).a
    else
      export PY_FRAMEWORK ?= 1
      ifdef PY_FRAMEWORK
        export NAN_PYTHON ?= /System/Library/Frameworks/Python.framework/Versions/2.5
        export NAN_PYTHON_VERSION ?= 2.5
        export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
        export NAN_PYTHON_LIB ?= -framework Python
      else
        export NAN_PYTHON ?= /sw
        export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
        export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
      endif
    endif

    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= $(LCGDIR)/tiff
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_GETTEXT_LIB ?= $(NAN_GETTEXT)/lib/libintl.a
    ifeq (($CPU), i386)
        export NAN_GETTEXT_LIB += $(NAN_GETTEXT)/lib/libintl.a
    endif
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_SDLCFLAGS ?= -I$(NAN_SDL)/include
    export NAN_SDLLIBS ?= $(NAN_SDL)/lib/libSDL.a -framework Cocoa -framework IOKit

    export NAN_OPENEXR ?= $(LCGDIR)/openexr
    export NAN_OPENEXR_INC ?= -I$(NAN_OPENEXR)/include -I$(NAN_OPENEXR)/include/OpenEXR
    export NAN_OPENEXR_LIBS ?= $(NAN_OPENEXR)/lib/libIlmImf.a $(NAN_OPENEXR)/lib/libHalf.a $(NAN_OPENEXR)/lib/libIex.a $(NAN_OPENEXR)/lib/libIlmThread.a
    
    export NAN_NO_KETSJI=false

    ifeq ($(CPU), i386)
      export WITH_OPENAL=false
    endif

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # enable quicktime by default on OS X
    export WITH_QUICKTIME ?= true

    # enable l10n
    export INTERNATIONAL ?= true

    export NAN_SAMPLERATE ?= $(LCGDIR)/samplerate
    export NAN_SAMPLERATE_LIBS ?= $(NAN_SAMPLERATE)/lib/libsamplerate.a 

  else
  ifeq ($(OS),freebsd)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export FREEDESKTOP ?= true

    export NAN_PYTHON ?= /usr/local
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    export NAN_OPENAL ?= /usr/local
    export NAN_JPEG ?= /usr/local
    export NAN_PNG ?= /usr/local
    export NAN_TIFF ?= /usr/local
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= /usr
    export NAN_NSPR ?= /usr/local
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_SDL ?= $(shell sdl-config --prefix)
    export NAN_SDLLIBS ?= $(shell sdl-config --libs)
    export NAN_SDLCFLAGS ?= $(shell sdl-config --cflags)

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # enable l10n
    # export INTERNATIONAL ?= true

  else
  ifeq ($(OS),irix)

    export ID = $(shell whoami)
    export HOST = $(shell /usr/bsd/hostname -s)
    #export NAN_NO_KETSJI=true
    export NAN_JUST_BLENDERDYNAMIC=true
    export NAN_PYTHON_VERSION ?= 3.1
    ifeq ($(IRIX_USE_GCC), true)
        export NAN_PYTHON ?= $(LCGDIR)/python_gcc
    else
        export NAN_PYTHON ?= $(LCGDIR)/python
    endif
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a -lpthread
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= $(LCGDIR)/tiff
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_ICONV ?= $(LCGDIR)/iconv
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_GETTEXT_LIB ?= $(NAN_GETTEXT)/lib/libintl.a $(NAN_ICONV)/lib/libiconv.a
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_SDLLIBS ?= $(NAN_SDL)/lib/libSDL.a
    export NAN_SDLCFLAGS ?= -I$(NAN_SDL)/include/SDL
    export NAN_FFMPEG ?= $(LCGDIR)/ffmpeg
    export NAN_FFMPEGLIBS = $(NAN_FFMPEG)/lib/libavformat.a $(NAN_FFMPEG)/lib/libavcodec.a $(NAN_FFMPEG)/lib/libswscale.a $(NAN_FFMPEG)/lib/libavutil.a $(NAN_FFMPEG)/lib/libavdevice.a $(NAN_FFMPEG)/lib/libogg.a $(NAN_FFMPEG)/lib/libfaad.a $(NAN_FFMPEG)/lib/libmp3lame.a $(NAN_FFMPEG)/lib/libvorbis.a $(NAN_FFMPEG)/lib/libx264.a $(NAN_FFMPEG)/lib/libfaac.a $(NAN_ZLIB)/lib/libz.a
    export NAN_FFMPEGCFLAGS ?= -I$(NAN_FFMPEG)/include -I$(NANBLENDERHOME)/extern/ffmpeg

    ifeq ($(IRIX_USE_GCC), true)
      export NAN_OPENEXR ?= $(LCGDIR)/openexr/gcc
    else
      export NAN_OPENEXR ?= $(LCGDIR)/openexr
    endif
    export NAN_OPENEXR_INC ?= -I$(NAN_OPENEXR)/include -I$(NAN_OPENEXR)/include/OpenEXR
    export NAN_OPENEXR_LIBS ?= $(NAN_OPENEXR)/lib/libIlmImf.a $(NAN_OPENEXR)/lib/libHalf.a $(NAN_OPENEXR)/lib/libIex.a $(NAN_OPENEXR)/lib/libIlmThread.a

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # enable l10n
    export INTERNATIONAL ?= true

  else
  ifeq ($(OS),linux)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export FREEDESKTOP ?= true

    export NAN_PYTHON ?= /usr
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    # Next line if for static python, nan_link.mk uses -lpython$(NAN_PYTHON_VERSION)
    #export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    export NAN_OPENAL ?= /usr
    export NAN_JPEG ?= /usr
    export NAN_PNG ?= /usr
    export NAN_TIFF ?= /usr
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr
    export NAN_ZLIB ?= /usr
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= /usr
    export NAN_GETTEXT ?= /usr
    export NAN_SDL ?= $(shell sdl-config --prefix)
    export NAN_SDLLIBS ?= $(shell sdl-config --libs)
    export NAN_SDLCFLAGS ?= $(shell sdl-config --cflags)
    export NAN_SAMPLERATE ?= /usr

ifneq ($(NAN_USE_FFMPEG_CONFIG), true)
    export NAN_FFMPEG ?= /usr
    export NAN_FFMPEGLIBS ?= -L$(NAN_FFMPEG)/lib -lavformat -lavcodec -lavutil -lswscale -lavdevice -ldts -lz
    export NAN_FFMPEGCFLAGS ?= -I$(NAN_FFMPEG)/include
endif

    ifeq ($(WITH_OPENEXR), true)
      export NAN_OPENEXR ?= $(shell pkg-config --variable=prefix OpenEXR )
      export NAN_OPENEXR_INC ?= $(shell pkg-config --cflags OpenEXR )
      export NAN_OPENEXR_LIBS ?= $(addprefix ${NAN_OPENEXR}/lib/lib,$(addsuffix .a,$(shell pkg-config --libs-only-l OpenEXR | sed -s "s/-l//g" )))
    endif

    # Uncomment the following line to use Mozilla inplace of netscape

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= /usr/include/mozilla
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    export INTERNATIONAL ?= true

    export WITH_BINRELOC ?= true

    # enable ffmpeg support
    ifndef NAN_NO_FFMPEG
      export WITH_FFMPEG ?= true
    endif

  else
  ifeq ($(OS),openbsd)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export FREEDESKTOP ?= true

    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= $(LCGDIR)/tiff
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_SDL ?= $(shell sdl-config --prefix)
    export NAN_SDLLIBS ?= $(shell sdl-config --libs)
    export NAN_SDLCFLAGS ?= $(shell sdl-config --cflags)

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    #export INTERNATIONAL ?= true

  else
  ifeq ($(OS),solaris)

    export ID = $(shell /usr/ucb/whoami)
    export HOST = $(shell hostname)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= /usr
    export NAN_TERRAPLAY ?=
    export NAN_MESA ?= /usr/X11
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_GETTEXT_LIB ?= $(NAN_GETTEXT)/lib/libintl.a $(NAN_GETTEXT)/lib/libiconv.a
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_SDLCFLAGS ?= -I$(NAN_SDL)/include/SDL
    export NAN_SDLLIBS ?= $(NAN_SDL)/lib/libSDL.a

    # this only exists at the moment for i386-64 CPU Types at the moment
    export NAN_OPENEXR ?= $(LCGDIR)/openexr
    export NAN_OPENEXR_INC ?= -I$(NAN_OPENEXR)/include -I$(NAN_OPENEXR)/include/OpenEXR
    export NAN_OPENEXR_LIBS ?= $(NAN_OPENEXR)/lib/libIlmImf.a $(NAN_OPENEXR)/lib/libHalf.a $(NAN_OPENEXR)/lib/libIex.a $(NAN_OPENEXR)/lib/libIlmThread.a -lrt

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    #export INTERNATIONAL ?= true

  else
  ifeq ($(OS),windows)

    export ID = $(LOGNAME)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_ICONV ?= $(LCGDIR)/iconv
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= $(LCGDIR)/tiff
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    ifeq ($(FREE_WINDOWS), true)
      export NAN_GETTEXT_LIB ?= $(NAN_GETTEXT)/lib/freegettext.a $(NAN_ICONV)/lib/freeiconv.a
      export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
      export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/lib25_vs2005/libpython25.a
      export NAN_FREETYPE ?= $(LCGDIR)/gcc/freetype
      export NAN_SDL ?= $(LCGDIR)/gcc/sdl
      export NAN_OPENEXR ?= $(LCGDIR)/gcc/openexr
      export NAN_OPENEXR_INC ?= -I$(NAN_OPENEXR)/include -I$(NAN_OPENEXR)/include/OpenEXR
      export NAN_OPENEXR_LIBS ?= $(NAN_OPENEXR)/lib/libIlmImf.a $(NAN_OPENEXR)/lib/libHalf.a $(NAN_OPENEXR)/lib/libIex.a
      export NAN_PTHREADS ?= $(LCGDIR)/pthreads
    else
      export NAN_GETTEXT_LIB ?= $(NAN_GETTEXT)/lib/gnu_gettext.lib $(NAN_ICONV)/lib/iconv.lib
      export NAN_PYTHON_BINARY ?= python
      export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python23.lib
      export NAN_FREETYPE ?= $(LCGDIR)/freetype
      export NAN_SDL ?= $(LCGDIR)/sdl
      export NAN_OPENEXR ?= $(LCGDIR)/openexr
      export NAN_OPENEXR_INC ?= -I$(NAN_OPENEXR)/include -I$(NAN_OPENEXR)/include/IlmImf -I$(NAN_OPENEXR)/include/Imath -I$(NAN_OPENEXR)/include/Iex
      export NAN_OPENEXR_LIBS ?= $(NAN_OPENEXR)/lib/IlmImf.lib $(NAN_OPENEXR)/lib/Half.lib $(NAN_OPENEXR)/lib/Iex.lib
    endif
    export NAN_SDLCFLAGS ?= -I$(NAN_SDL)/include

    export NAN_WINTAB ?= $(LCGDIR)/wintab

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.
    export NAN_PYTHON_BINARY ?= python
    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    export INTERNATIONAL ?= true

    # enable quicktime support
    # export WITH_QUICKTIME ?= true

  else # Platform not listed above

    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 3.1
    export NAN_PYTHON_BINARY ?= python
    export NAN_PYTHON_LIB ?= $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a

    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_TIFF ?= $(LCGDIR)/tiff
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext
    export NAN_SDL ?= $(shell sdl-config --prefix)
    export NAN_SDLLIBS ?= $(shell sdl-config --libs)
    export NAN_SDLCFLAGS ?= $(shell sdl-config --cflags)

    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    #export INTERNATIONAL ?= true

  endif # windows + fallback
  endif # solaris
  endif # openbsd
  endif # linux
  endif # irix
  endif # freebsd
  endif # darwin

endif # CONFIG_GUESS

# Don't want to build the gameengine?
ifeq ($(NAN_NO_KETSJI), true)
   export NAN_JUST_BLENDERDYNAMIC=true
endif
