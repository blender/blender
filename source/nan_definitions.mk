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
    export SRCHOME ?= $(NANBLENDERHOME)/source
    export NAN_LIBDIR ?= $(NANBLENDERHOME)/lib
    export NAN_OBJDIR ?= $(NANBLENDERHOME)/obj
    export NAN_PYTHON ?= $(LCGDIR)/python
    export NAN_PYTHON_VERSION ?= 2.0
    export NAN_PYTHON_BINARY ?= $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_MXTEXTTOOLS ?= $(shell $(NAN_PYTHON_BINARY) -c 'import mx; print mx.__path__[0]')/TextTools/mxTextTools/mxTextTools.so 
    export NAN_OPENAL ?= $(LCGDIR)/openal
    export NAN_FMOD ?= $(LCGDIR)/fmod
    export NAN_JPEG ?= $(LCGDIR)/jpeg
    export NAN_PNG ?= $(LCGDIR)/png
    export NAN_SDL ?= $(LCGDIR)/sdl
    export NAN_TERRAPLAY ?= $(LCGDIR)/terraplay
    export NAN_MESA ?= /usr/src/Mesa-3.1
    export NAN_MOTO ?= $(LCGDIR)/moto
    export NAN_SOLID ?= $(SRCHOME)/sumo/SOLID-3.0
    export NAN_SUMO ?= $(SRCHOME)/gameengine/Physics/Sumo
    export NAN_FUZZICS ?= $(SRCHOME)/gameengine/Physics/Sumo/Fuzzics
    export NAN_ODE ?= $(SRCHOME)/ode
    export NAN_OPENSSL ?= $(LCGDIR)/openssl
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
    export NAN_ZLIB ?= $(LCGDIR)/zlib
    export NAN_BMFONT ?= $(LCGDIR)/bmfont
    # Uncomment the following line to use Mozilla inplace of netscape
    # CPPFLAGS +=-DMOZ_NOT_NET
    # Location of MOZILLA/Netscape header files...
    export NAN_MOZILLA_INC ?= $(LCGDIR)/mozilla/include
    export NAN_MOZILLA_LIB ?= $(LCGDIR)/mozilla/lib/
    # Will fall back to look in NAN_MOZILLA_INC/nspr and NAN_MOZILLA_LIB
    # if this is not set.
    export NAN_NSPR ?= $(LCGDIR)/nspr

    export NAN_BUILDINFO = true

    # Be paranoid regarding library creation (do not update archives)
    export NAN_PARANOID = true

    # Library Config_Guess DIRectory
    export LCGDIR = $(NAN_LIBDIR)/$(CONFIG_GUESS)

    # Object Config_Guess DIRectory
    export OCGDIR = $(NAN_OBJDIR)/$(CONFIG_GUESS)

    export CONFIG_GUESS := $(shell ${SRCHOME}/tools/guess/guessconfig)
    export OS := $(shell echo ${CONFIG_GUESS} | sed -e 's/-.*//')
    export OS_VERSION := $(shell echo ${CONFIG_GUESS} | sed -e 's/^[^-]*-//' -e 's/-[^-]*//')
    export CPU := $(shell echo ${CONFIG_GUESS} | sed -e 's/^[^-]*-[^-]*-//')
    export MAKE_START := $(shell date "+%H:%M:%S %d-%b-%Y")

  ifeq ($(OS),beos)
    ID = $(USER)
    HOST = $(HOSTNAME)
  endif
  ifeq ($(OS),darwin)
    ID = $(shell whoami)
    HOST = $(shell hostname -s)
    # Override libraries locations to use fink installed libraries
    export NAN_OPENSSL = /sw
    export NAN_JPEG = /sw
    export NAN_PNG = /sw
    export NAN_ODE = $(LCGDIR)/ode
 	# Override common python settings so that the python that comes with 
	# OSX 10.2 in /usr/local/ is used.
    export NAN_PYTHON = /usr/local
    export NAN_PYTHON_VERSION = 2.2
    export NAN_PYTHON_BINARY = $(NAN_PYTHON)/bin/python$(NAN_PYTHON_VERSION)
    export NAN_MXTEXTTOOLS = $(shell $(NAN_PYTHON_BINARY) -c 'import mx; print mx.__path__[0]')/TextTools/mxTextTools/mxTextTools.so 
  endif
  ifeq ($(OS),freebsd)
    ID = $(shell whoami)
    HOST = $(shell hostname -s)
  endif
  ifeq ($(OS),irix)
    ID = $(shell whoami)
    HOST = $(shell /usr/bsd/hostname -s)
  endif
  ifeq ($(OS),linux)
    ID = $(shell whoami)
    HOST = $(shell hostname -s)
  endif
  ifeq ($(OS),openbsd)
    ID = $(shell whoami)
    HOST = $(shell hostname -s)
  endif
  ifeq ($(OS),solaris)
    ID = $(shell /usr/ucb/whoami)
    HOST = $(shell hostname)
  endif
  ifeq ($(OS),windows)
    ID = $(LOGNAME)
  endif
    export ID HOST

endif



