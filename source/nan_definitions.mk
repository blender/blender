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
# set some defaults when these are not overruled (?=) by environment variables
#

ifndef CONFIG_GUESS
  ifeq (debug, $(findstring debug, $(MAKECMDGOALS)))
    ifeq (all, $(findstring all, $(MAKECMDGOALS)))
all debug::
      ERRTXT = "ERROR: all and debug targets cannot be used together anymore"
      ERRTXT += "Use something like ..make all && make debug.. instead"
      $(error $(ERRTXT))
    endif
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
    export NAN_MOTO ?= $(LCGDIR)/moto
    export NAN_SOLID ?= $(SRCHOME)/sumo/SOLID-3.0
    export NAN_SUMO ?= $(SRCHOME)/gameengine/Physics/Sumo
    export NAN_FUZZICS ?= $(SRCHOME)/gameengine/Physics/Sumo/Fuzzics
    export NAN_BLENKEY ?= $(LCGDIR)/blenkey
    export NAN_DECIMATION ?= $(LCGDIR)/decimation
    export NAN_GUARDEDALLOC ?= $(LCGDIR)/guardedalloc
    export NAN_IKSOLVER ?= $(LCGDIR)/iksolver
    export NAN_BSP ?= $(LCGDIR)/bsp
    export NAN_STRING ?= $(LCGDIR)/string
    export NAN_MEMUTIL ?= $(LCGDIR)/memutil
    export NAN_CONTAINER ?= $(LCGDIR)/container
    export NAN_ACTION ?= $(LCGDIR)/action
    export NAN_IMG ?= $(LCGDIR)/img
    export NAN_GHOST ?= $(LCGDIR)/ghost
    export NAN_TEST_VERBOSITY ?= 1
    export NAN_BMFONT ?= $(LCGDIR)/bmfont
    ifeq ($(FREE_WINDOWS), true)
      export NAN_FTGL ?= $(LCGDIR)/gcc/ftgl
    else
      export NAN_FTGL ?= $(LCGDIR)/ftgl
    endif
	export NAN_SDLLIBS ?= $(shell sdl-config --libs)
	export NAN_SDLCFLAGS ?= $(shell sdl-config --cflags)


  # Platform Dependent settings go below:

  ifeq ($(OS),beos)

    export ID = $(USER)
    export HOST = $(HOSTNAME)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 2.0
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext	

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    #export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),darwin)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export NAN_PYTHON ?= /sw
    export NAN_PYTHON_VERSION ?= 2.2
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= /sw
    export NAN_PNG ?= /sw
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= /sw
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),freebsd)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export NAN_PYTHON ?= /usr/local
    export NAN_PYTHON_VERSION ?= 2.2
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= /usr/local
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= /usr/local
    export NAN_PNG ?= /usr/local
    export NAN_SDL ?= /usr/local
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= /usr
    export NAN_NSPR ?= /usr/local
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    # export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),irix)

    export ID = $(shell whoami)
    export HOST = $(shell /usr/bsd/hostname -s)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 2.1
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= /usr/freeware
    export NAN_NSPR ?= /usr/local/apps/openblender/nspr/target/dist
    export NAN_FREETYPE ?= /usr/freeware
    export NAN_GETTEXT ?= /usr/freeware

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),linux)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export NAN_PYTHON ?= /usr
    export NAN_PYTHON_VERSION ?= 2.2
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= /usr
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= /usr
    export NAN_PNG ?= /usr
    export NAN_SDL ?= /usr
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr
    export NAN_ZLIB ?= /usr
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= /usr
    export NAN_GETTEXT ?= /usr

    # Uncomment the following line to use Mozilla inplace of netscape
    export CPPFLAGS += -DMOZ_NOT_NET
    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= /usr/include/mozilla
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.

    export NAN_BUILDINFO ?= true
    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID ?= true

    # l10n
    #export INTERNATIONAL ?= true

    # enable freetype2 support for text objects
    #export WITH_FREETYPE2 ?= true


  else
  ifeq ($(OS),openbsd)

    export ID = $(shell whoami)
    export HOST = $(shell hostname -s)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 2.0
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    #export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),solaris)

    export ID = $(shell /usr/ucb/whoami)
    export HOST = $(shell hostname)
    export NAN_PYTHON ?= /usr/local
    export NAN_PYTHON_VERSION ?= 2.2
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_OPENAL ?= /usr/local
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= /usr/local
    export NAN_PNG ?= /usr/local
    export NAN_SDL ?= /usr/local
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?=
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= /usr
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    #export WITH_FREETYPE2 ?= true

  else
  ifeq ($(OS),windows)

    export ID = $(LOGNAME)
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_ICONV ?= $(LCGDIR)/iconv
    export NAN_PYTHON_VERSION ?= 2.2
    ifeq ($(FREE_WINDOWS), true)
      export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
      export NAN_FREETYPE ?= $(LCGDIR)/gcc/freetype
      export NAN_ODE ?= $(LCGDIR)/gcc/ode
    else
      export NAN_PYTHON_BINARY ?= python
      export NAN_FREETYPE ?= $(LCGDIR)/freetype
      export NAN_ODE ?= $(LCGDIR)/ode
    endif
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    export WITH_FREETYPE2 ?= true
    
    # enable quicktime support
    # export WITH_QUICKTIME ?= true

  else # Platform not listed above

    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 2.0
	export NAN_PYTHON_BINARY ?= python
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_ODE ?= $(LCGDIR)/ode
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_NSPR ?= $(LCGDIR)/nspr
    export NAN_FREETYPE ?= $(LCGDIR)/freetype
    export NAN_GETTEXT ?= $(LCGDIR)/gettext

    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
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

    # enable freetype2 support for text objects
    #export WITH_FREETYPE2 ?= true
  endif
endif
endif
endif
endif
endif
endif
endif
endif
