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
# Top level makefile. Start looping through subdirs and link....
#

ifndef NANENV
    export NANENV := $(shell env | grep NAN_)
endif

include nan_definitions.mk

DIRS = creator blender kernel gameengine

ifeq ($(OS),windows)
    DIRS += icons
endif

########## buildinfo kludge ###################
CPPFLAGS += -I../Physics/common
CPPFLAGS += -I../Physics/Dummy
ifdef NAN_BUILDINFO
    BUILDINFO_O = $(DIR)/$(DEBUG_DIR)buildinfo.o
    BUILDINFO_C = $(SRCHOME)/creator/buildinfo.c
    BUILD_DATE := $(shell date "+%Y-%m-%d")
    BUILD_TIME := $(shell date "+%H:%M:%S")
endif

############# set pyplayerlib ##################

PYPLAYERLIB ?= $(PYLIB)

############# libraries ##################
# COMLIB COMmon LIBraries for all targets
# BPLIB Browser Plugin LIBraries
# SPLIB1 Standalone Player LIBraries (pre COMLIB)
# SPLIB Standalone Player LIBraries (post COMLIB)
# PULIB PUblisher LIBraries
# GRPLIB Gui and Render LIBraries for Publisher
# LLIBS dynamic system libraries
# SADD Static OpenGL libraries
# DADD Dynamic OpenGL libraries
# NSPLUGLIB PLUGin LIBraries for ns-style api
# PLUGAPPLIB libs that form the application to be plugged in
# Note: the order is important here

    GRPLIB = $(OCGDIR)/creator/$(DEBUG_DIR)libcreator.a
    GRPLIB += $(OCGDIR)/blender/src/$(DEBUG_DIR)libsrcpublisher.a
    GRPLIB += $(NAN_DECIMATION)/lib/libdecimation.a
    GRPLIB += $(NAN_BSP)/lib/$(DEBUG_DIR)libbsp.a
    GRPLIB += $(NAN_SOUNDSYSTEM)/lib/$(DEBUG_DIR)libSoundSystem.a
    GRPLIB += $(NAN_GHOST)/lib/$(DEBUG_DIR)libghost.a
    GRPLIB += $(NAN_STRING)/lib/$(DEBUG_DIR)libstring.a
    GRPLIB += $(OCGDIR)/blender/img/$(DEBUG_DIR)libimg.a
    GRPLIB += $(OCGDIR)/blender/renderconverter/$(DEBUG_DIR)librenderconverter.a
    GRPLIB += $(OCGDIR)/blender/render/$(DEBUG_DIR)librender.a
    GRPLIB += $(OCGDIR)/blender/radiosity/$(DEBUG_DIR)libradiosity.a

    GRPLIB += $(OCGDIR)/blender/python/$(DEBUG_DIR)libpython.a

# nlin: the reason that some libraries appear more than once below is
# to handle circular dependencies in linking among libraries... some
# linkers (e.g. under Linux) need libs to be specified multiple times
# to properly resolve circular dependencies. ugly, but it works...
# the repeat entries could probably be trimmed down.
    COMLIB = $(OCGDIR)/blender/blenkernel/$(DEBUG_DIR)libblenkernel.a
    COMLIB += $(OCGDIR)/blender/blenloader/$(DEBUG_DIR)libblenloader.a
    COMLIB += $(OCGDIR)/blender/blenpluginapi/$(DEBUG_DIR)libblenpluginapi.a
    COMLIB += $(OCGDIR)/blender/imbuf/$(DEBUG_DIR)libimbuf.a
    COMLIB += $(OCGDIR)/blender/blenlib/$(DEBUG_DIR)libblenlib.a
    COMLIB += $(OCGDIR)/blender/avi/$(DEBUG_DIR)libavi.a
    COMLIB += $(NAN_JPEG)/lib/libjpeg.a
    COMLIB += $(OCGDIR)/gameengine/bloutines/$(DEBUG_DIR)libbloutines.a
    COMLIB += $(OCGDIR)/gameengine/blconverter/$(DEBUG_DIR)libblconverter.a
    COMLIB += $(OCGDIR)/gameengine/blphys/common/$(DEBUG_DIR)libcommon.a
    COMLIB += $(OCGDIR)/gameengine/blphys/blode/$(DEBUG_DIR)libblode.a
    COMLIB += $(NAN_ODE)/lib/libode.a
#    COMLIB += $(OCGDIR)/gameengine/blphys/sumo/$(DEBUG_DIR)libsumo.a
    COMLIB += $(OCGDIR)/gameengine/blphys/dummy/$(DEBUG_DIR)libdummy.a
    COMLIB += $(OCGDIR)/gameengine/blphys/common/$(DEBUG_DIR)libcommon.a
#    COMLIB += $(OCGDIR)/gameengine/blphys/sumo/$(DEBUG_DIR)libsumo.a
    COMLIB += $(OCGDIR)/gameengine/blphys/dummy/$(DEBUG_DIR)libdummy.a
    COMLIB += $(OCGDIR)/gameengine/ketsji/$(DEBUG_DIR)libketsji.a
    COMLIB += $(OCGDIR)/gameengine/blphys/common/$(DEBUG_DIR)libcommon.a
    COMLIB += $(OCGDIR)/gameengine/blphys/blode/$(DEBUG_DIR)libblode.a
#    COMLIB += $(OCGDIR)/gameengine/blphys/sumo/$(DEBUG_DIR)libsumo.a
    COMLIB += $(OCGDIR)/gameengine/blphys/dummy/$(DEBUG_DIR)libdummy.a
    COMLIB += $(OCGDIR)/gameengine/blphys/common/$(DEBUG_DIR)libcommon.a
#    COMLIB += $(OCGDIR)/gameengine/blphys/sumo/$(DEBUG_DIR)libsumo.a
    COMLIB += $(OCGDIR)/gameengine/blphys/dummy/$(DEBUG_DIR)libdummy.a
    COMLIB += $(OCGDIR)/gameengine/ketsji/$(DEBUG_DIR)libketsji.a
    COMLIB += $(OCGDIR)/gameengine/logic/$(DEBUG_DIR)liblogic.a
    COMLIB += $(OCGDIR)/gameengine/rasterizer/$(DEBUG_DIR)librasterizer.a
    COMLIB += $(OCGDIR)/gameengine/OpenGLrasterizer/$(DEBUG_DIR)libOpenGLrasterizer.a
    COMLIB += $(OCGDIR)/gameengine/expression/$(DEBUG_DIR)libexpression.a
    COMLIB += $(OCGDIR)/gameengine/scenegraph/$(DEBUG_DIR)libscenegraph.a
#    COMLIB += $(OCGDIR)/sumo/$(DEBUG_DIR)libfuzzics.a
#    COMLIB += $(OCGDIR)/sumo/$(DEBUG_DIR)libsolid.a
    COMLIB += $(NAN_MOTO)/lib/libmoto.a
    COMLIB += $(NAN_SND_LIBS)
    COMLIB += $(OCGDIR)/kernel/gen_system/$(DEBUG_DIR)libgen_system.a
    COMLIB += $(OCGDIR)/kernel/gen_messaging/$(DEBUG_DIR)libgen_messaging.a
    COMLIB += $(OCGDIR)/gameengine/ketsji/KXNetwork/$(DEBUG_DIR)libKXNetwork.a
    COMLIB += $(OCGDIR)/gameengine/Network/$(DEBUG_DIR)libNetwork.a
    COMLIB += $(OCGDIR)/gameengine/Network/LoopBackNetwork/$(DEBUG_DIR)libLoopBackNetwork.a
    COMLIB += $(NAN_GUARDEDALLOC)/lib/libguardedalloc.a
    COMLIB += $(NAN_BMFONT)/lib/$(DEBUG_DIR)libbmfont.a
    COMLIB += $(NAN_PNG)/lib/libpng.a
  ifeq ($(WITH_QUICKTIME), true)
    COMLIB += $(OCGDIR)/blender/blenderqt/$(DEBUG_DIR)libblenderqt.a
  endif
  ifeq ($(WITH_FREETYPE2), true)
    ifeq ($(OS), windows)
		ifeq ($(FREE_WINDOWS), true)
			COMLIB += $(NAN_FREETYPE)/lib/libfreetype.a
		else
			COMLIB += $(NAN_FREETYPE)/lib/freetype2ST.lib
		endif
    else
      ifeq ($(OS), irix)
        COMLIB += $(NAN_FREETYPE)/lib32/libfreetype.a
       else
        COMLIB += $(NAN_FREETYPE)/lib/libfreetype.a
      endif
    endif
  endif
ifeq ($(INTERNATIONAL), true)
    COMLIB += $(OCGDIR)/blender/ftfont/$(DEBUG_DIR)libftfont.a
    ifeq ($(OS), windows)
        ifeq ($(FREE_WINDOWS), true)
          COMLIB += $(NAN_GETTEXT)/lib/freegettext.a
          COMLIB += $(NAN_FTGL)/lib/libftgl.a
          #COMLIB += $(NAN_ICONV)/lib/freeiconv.a
        else
          COMLIB += $(NAN_GETTEXT)/lib/gnu_gettext.lib
          COMLIB += $(NAN_FTGL)/lib/ftgl_static_ST.lib
          COMLIB += $(NAN_FREETYPE)/lib/freetype2ST.lib
          COMLIB += $(NAN_ICONV)/lib/iconv.lib
        endif
    else
        COMLIB += $(NAN_FTGL)/lib/libftgl.a
        ifeq ($(OS), irix)
          COMLIB += $(NAN_FREETYPE)/lib32/libfreetype.a
          COMLIB += $(NAN_FREETYPE)/lib32/libintl.a
        else
          COMLIB += $(NAN_FREETYPE)/lib/libfreetype.a
        endif
    endif
	ifeq ($(OS), darwin)
	    COMLIB += $(NAN_GETTEXT)/lib/libintl.a
	endif
endif

ifeq ($(OS), irix)
	COMLIB += $(NAN_SDL)/lib/libSDL.a
endif
ifeq ($(OS), windows)
    ifeq ($(FREE_WINDOWS), true)
	    COMLIB += $(NAN_SDL)/lib/libSDL.a
    else
	    COMLIB += $(NAN_SDL)/lib/SDL.lib
    endif
endif

    PULIB = $(NAN_IKSOLVER)/lib/libiksolver.a
    PULIB += $(NAN_MOTO)/lib/libmoto.a
    PULIB += $(OCGDIR)/blender/readblenfile/$(DEBUG_DIR)libreadblenfile.a
    PULIB += $(OCGDIR)/blender/readstreamglue/$(DEBUG_DIR)libreadstreamglue.a
    PULIB += $(OCGDIR)/blender/writestreamglue/$(DEBUG_DIR)libwritestreamglue.a
    # toggle 2 writestreamglue libs for the Uber-Publisher :)
    #PULIB += $(OCGDIR)/blender/writestreamglueSTUB/$(DEBUG_DIR)libwritestreamglueSTUB.a
    PULIB += $(OCGDIR)/blender/inflate/$(DEBUG_DIR)libinflate.a
    PULIB += $(OCGDIR)/blender/deflate/$(DEBUG_DIR)libdeflate.a
    PULIB += $(OCGDIR)/blender/writeblenfile/$(DEBUG_DIR)libwriteblenfile.a
    PULIB += $(OCGDIR)/blender/writestreamglue/$(DEBUG_DIR)libwritestreamglue.a
    # toggle 2 writestreamglue libs for the Uber-Publisher :)
    #PULIB += $(OCGDIR)/blender/writestreamglueSTUB/$(DEBUG_DIR)libwritestreamglueSTUB.a
    PULIB += $(OCGDIR)/blender/readstreamglue/$(DEBUG_DIR)libreadstreamglue.a # KEY_dependkludge fix.. Remove me
    PULIB += $(OCGDIR)/blender/src/$(DEBUG_DIR)libsrcpublisher.a

    SPLIB1 = $(OCGDIR)/gameengine/GamePlayer/ghost/$(DEBUG_DIR)libghost.a
    SPLIB1 += $(OCGDIR)/gameengine/GamePlayer/common/$(DEBUG_DIR)libcommon.a
    SPLIB1 += $(NAN_STRING)/lib/$(DEBUG_DIR)libstring.a
    SPLIB1 += $(NAN_GHOST)/lib/$(DEBUG_DIR)libghost.a

    SPLIB = $(OCGDIR)/blender/makesdna/$(DEBUG_DIR)DNA.o
    SPLIB += $(OCGDIR)/blender/readblenfile/$(DEBUG_DIR)libreadblenfile.a
    SPLIB += $(OCGDIR)/blender/readstreamglue/$(DEBUG_DIR)libreadstreamglue.a
    # this is used for the plugin. It uses some things from libz.a, 
    # but somehow it consistently fails to resolve these symbols... or 
    # can I just not check them? nm claims they aren't...
    SPLIB += $(OCGDIR)/blender/inflate/$(DEBUG_DIR)libinflate.a
    SPLIB += $(OCGDIR)/blender/writestreamglueSTUB/$(DEBUG_DIR)libwritestreamglueSTUB.a
    SPLIB += $(OCGDIR)/blender/blenkernel/blenkernel_blc/$(DEBUG_DIR)libblenkernel_blc.a

    # These three need to be explicitly mentioned on the cl, because 
    # if they are offered as a lib, they are optimized away. (nzc)
    NSPLUGLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)UnixShell.o
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)stubs.o
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)winstubs.o
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)Blender3DPlugin_native_implementation.o

    # Here you can configure what sort of test to make for the plugin.
#    PLUGTESTLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/test/$(DEBUG_DIR)libns_api_test_stub.a
#    PLUGTESTLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/test/$(DEBUG_DIR)libns_api_test_colorcycle.a
#    PLUGTESTLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/test/$(DEBUG_DIR)libns_api_test_gears.a
    PLUGTESTLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/test/$(DEBUG_DIR)libns_api_test_threaded_gears.a

# XPCOM is done only for Linux for now
ifeq ($(OS),$(findstring $(OS), "linux"))
  ifeq ($(CPU),i386)
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)XPH_xpcom_hooks.o
  endif
endif

    PLUGAPPLIB = $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)libplugin_ketsji_hooks.a

ifeq ($(OS),$(findstring $(OS), "freebsd linux irix solaris"))
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)PLG_X11_windowing.o
    PLUGAPPLIB += $(OCGDIR)/gameengine/GamePlayer/common/unix/$(DEBUG_DIR)libunix.a
endif
ifeq ($(OS),$(findstring $(OS), "windows"))
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)PLG_MSWindows_windowing.o
    NSPLUGLIB += $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)npB3DPlg.res
    PLUGAPPLIB += $(OCGDIR)/gameengine/GamePlayer/common/windows/$(DEBUG_DIR)libwindows.a
endif
    PLUGAPPLIB += $(OCGDIR)/gameengine/GamePlayer/common/$(DEBUG_DIR)libcommon.a


ifeq ($(OS),$(findstring $(OS), "linux"))
  ifeq ($(CPU),i386)
    PLUGAPPLIB_XPLINK = $(OCGDIR)/gameengine/GamePlayer/netscape/src/$(DEBUG_DIR)_Blender3DPlugin_implementation_.o 
  endif
endif

    # A bit silly... but needed to resolve symbols.
    PLUGREMLIB = $(NAN_STRING)/lib/$(DEBUG_DIR)libstring.a

######################## OS dependencies (alphabetic!) ################

include nan_link.mk

ifeq ($(OS),beos)
    BINTARGETS = blenderdynamic
endif

ifeq ($(OS),darwin)
    BINTARGETS = blenderdynamic.app
    BINTARGETS += blenderplayer.app
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    # Next line would enable MxTextTools (for VRML2 import) 
    # but results in a link error
    # PYLIB += $(NAN_MXTEXTTOOLS) 
endif

ifeq ($(OS),freebsd)
  ifeq ($(OS_VERSION),$(findstring $(OS_VERSION), "3.4 4.0"))
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderstatic
  else
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderplayer
    BINTARGETS += plugin
  endif
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    PYLIB += $(NAN_MXTEXTTOOLS) 
endif

ifeq ($(OS),irix)
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderplayer
    BINTARGETS += plugin
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    PYLIB += $(NAN_MXTEXTTOOLS)
endif

ifeq ($(OS),linux)
  ifeq ($(CPU),alpha)
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderplayer
  endif
  ifeq ($(CPU),i386)
#    BINTARGETS = plugin
    # BINTARGETS += ptest
#    BINTARGETS += xplink
    BINTARGETS += blenderdynamic
#    BINTARGETS += blenderplayer
#    BINTARGETS += blenderstatic
  endif
  ifeq ($(CPU),powerpc)
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderstatic
  endif
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    PYLIB += $(NAN_MXTEXTTOOLS)
endif

ifeq ($(OS),openbsd)
    BINTARGETS = blenderstatic
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
endif

ifeq ($(OS),solaris)
    BINTARGETS = blenderdynamic
    BINTARGETS += blenderplayer
  ifeq ($(CPU),sparc)
    BINTARGETS += plugin
  endif
    PYLIB = $(NAN_PYTHON)/lib/python$(NAN_PYTHON_VERSION)/config/libpython$(NAN_PYTHON_VERSION).a
    PYLIB += $(NAN_MXTEXTTOOLS)
    PYLIB += $(NAN_ZLIB)/lib/libz.a

    PULIB += $(NAN_ZLIB)/lib/libz.a
    SPLIB += $(NAN_ZLIB)/lib/libz.a
endif

# OpenAL libs are already compiled as shared code! Check FMod if we switch to that. (nzc)


# Some vars to keep the rest of this section mostly readable
# in an 80 char term
SOUNDSYSTEM = $(OCGDIR)/intern/SoundSystem/$(DEBUG_DIR)libSoundSystem.a
DUMMYSOUND  = $(OCGDIR)/intern/DummySoundSystem/$(DEBUG_DIR)libDummySoundSystem.a
OPENALSOUND = $(OCGDIR)/intern/OpenALSoundSystem/$(DEBUG_DIR)libOpenALSoundSystem.a

# Some kooky logic going on here ...
ifeq ($(NAN_NO_OPENAL), true)
    NAN_SND_LIBS =  $(SOUNDSYSTEM)
    NAN_SND_LIBS += $(DUMMYSOUND)
    NAN_SND_LIBS += $(SOUNDSYSTEM)
else
  ifeq ($(OS),$(findstring $(OS), "freebsd linux windows"))
    ifeq ($(CPU),i386)
      ifeq ($(OS),freebsd)
        NAN_SND_LIBS =  $(SOUNDSYSTEM)
        NAN_SND_LIBS += $(DUMMYSOUND)
        NAN_SND_LIBS += $(OPENALSOUND)
        NAN_SND_LIBS += $(NAN_OPENAL)/lib/libopenal.a
        NAN_SND_LIBS += $(SOUNDSYSTEM)
      else
        NAN_SND_LIBS =  $(SOUNDSYSTEM)
        NAN_SND_LIBS += $(DUMMYSOUND)
        NAN_SND_LIBS += $(OPENALSOUND)
        NAN_SND_LIBS += $(NAN_OPENAL)/lib/libopenal.a
        NAN_SND_LIBS += $(SOUNDSYSTEM)
      endif
    else
      ifeq ($(OS),windows)
        NAN_SND_LIBS =  $(SOUNDSYSTEM)
        NAN_SND_LIBS += $(DUMMYSOUND)
        NAN_SND_LIBS += $(OPENALSOUND)
        NAN_SND_LIBS += $(NAN_OPENAL)/lib/openal_static.lib
        NAN_SND_LIBS += $(SOUNDSYSTEM)
      else
        NAN_SND_LIBS =  $(SOUNDSYSTEM)
        NAN_SND_LIBS += $(DUMMYSOUND)
        NAN_SND_LIBS += $(SOUNDSYSTEM)
      endif
    endif
  else
    ifeq ($(OS),darwin)
      NAN_SND_LIBS =  $(SOUNDSYSTEM)
      NAN_SND_LIBS += $(DUMMYSOUND)
      NAN_SND_LIBS += $(SOUNDSYSTEM)
    else
      NAN_SND_LIBS =  $(SOUNDSYSTEM)
      NAN_SND_LIBS += $(DUMMYSOUND)
      NAN_SND_LIBS += $(SOUNDSYSTEM)
    endif
  endif
endif

ifeq ($(OS),windows)
	ifeq ($(FREE_WINDOWS),true)
		PYLIB = $(NAN_PYTHON)/lib/freepy.a
    else
		PYLIB = $(NAN_PYTHON)/lib/python22.lib
    endif
    
    PYPLAYERLIB = $(NAN_PYTHON)/static/*.obj
    PYPLAYERLIB = $(PYLIB)

    # Might need to change this to $(NAN_MOZILLA_LIB)/nspr4.lib
    NSPLUGLIB += $(NAN_NSPR)/lib/nspr4.lib

    BINTARGETS = blenderdynamic
	ifneq ($(FREE_WINDOWS),true)
	    BINTARGETS += blenderplayer
		BINTARGETS += blenderdynplayer
	    BINTARGETS += plugin
	endif

    PULIB += $(OCGDIR)/blender/icons/$(DEBUG_DIR)winblender.res
    PULIB += $(NAN_ZLIB)/lib/libz.a
    SPLIB += $(OCGDIR)/blender/icons/$(DEBUG_DIR)winblender.res
    SPLIB += $(NAN_ZLIB)/lib/libz.a

  ifdef NAN_PLAYERSTATIC
    PYPLAYERLIB = $(NAN_PYTHON)/static/python20.lib
    BINTARGETS = blenderplayer

    NAN_SND_LIBS = $(OCGDIR)/intern/SoundSystem/$(DEBUG_DIR)libSoundSystem.a
    NAN_SND_LIBS += $(OCGDIR)/intern/DummySoundSystem/$(DEBUG_DIR)libDummySoundSystem.a
    NAN_SND_LIBS += $(OCGDIR)/intern/OpenALSoundSystem/$(DEBUG_DIR)libOpenALSoundSystem.a
    NAN_SND_LIBS += $(NAN_OPENAL)/lib/openal_static.lib
    NAN_SND_LIBS += $(OCGDIR)/intern/SoundSystem/$(DEBUG_DIR)libSoundSystem.a

    PYINCLUDEOK := $(shell diff -qbB gameengine/Ketsji/KX_Python.h gameengine/Ketsji/KX_Python_static.h)
    ifneq (x$(PYINCLUDEOK),x)
      DUMMY := $(shell cat gameengine/Ketsji/KX_Python_static.h > gameengine/Ketsji/KX_Python.h)
    endif
  else
    PYINCLUDEOK := $(shell diff -qbB gameengine/Ketsji/KX_Python.h gameengine/Ketsji/KX_Python_dynamic.h)
    ifneq (x$(PYINCLUDEOK),x)
      DUMMY := $(shell cat gameengine/Ketsji/KX_Python_dynamic.h > gameengine/Ketsji/KX_Python.h)
    endif
  endif
endif

# prepare for NAN_BUILDINFO compile at the *sigh* link rules below

ifdef NAN_BUILDINFO
    CSRCS =
    ALLTARGETS =
    include nan_compile.mk
endif

################## target rules ########################

all debug ::
	@echo "****> Make $@ by $(ID)@$(HOST) at $(MAKE_START) on $(CONFIG_GUESS)"
    ifdef NANENV
	@for n in $(NANENV); do \
	    echo "      $$n"; \
	done
    endif

DIR = $(OCGDIR)
SOURCEDIR = source

include nan_subdirs.mk

################## target rules ########################

all:: link

debug debuglink::
	@$(MAKE) link DEBUG_DIR="debug/" NANENV=$(NANENV)

clean:: linkclean debuglinkclean

link: $(BINTARGETS)
	@echo "****> Build $(MAKE_START) - `date '+%H:%M:%S %d-%b-%Y'`"
    ifdef NANENV
	@for n in $(NANENV); do \
	    echo "      $$n"; \
	done
    endif

linkclean:
	@$(RM) $(DIR)/blenderstatic* \
		$(DIR)/blenderplayer* \
		$(DIR)/bin/*

debuglinkclean:
	@$(RM) $(DIR)/debug/blenderstatic* \
		$(DIR)/debug/blenderplayer* \
		$(DIR)/debug/bin/*

# Shortcut for Tinderbox release builds. Mortals don't use this.
release:
	@$(MAKE) -C ../release/ all || exit 1;

############### Linker rules ###############

blenderstatic: $(DIR)/$(DEBUG_DIR)bin/blenderstatic
blenderdynamic: $(DIR)/$(DEBUG_DIR)bin/blender$(EXT)
blenderplayer: $(DIR)/$(DEBUG_DIR)bin/blenderplayer$(EXT)
blenderdynplayer: $(DIR)/$(DEBUG_DIR)bin/blenderdynplayer$(EXT)

blenderplayer.app: blenderplayer
	@$(MAKE) -C darwin/ APPLICATION=blenderplayer

blenderdynamic.app: blenderdynamic
	@$(MAKE) -C darwin/ APPLICATION=blender

# XPCOM connector only for linux so far...
ifeq ($(OS),linux)
    xplink: $(DIR)/$(DEBUG_DIR)bin/Blender3DPlugin$(SOEXT)
endif

# Moz/NS plugin:
ifeq ($(OS),$(findstring $(OS), "freebsd linux irix solaris"))
    plugin: $(DIR)/$(DEBUG_DIR)bin/npBlender3DPlugin$(SOEXT)
    ptest: $(DIR)/$(DEBUG_DIR)bin/npTestPlugin$(SOEXT)
endif
# Windows needs a 8.3 name... Why? Dunno...
ifeq ($(OS),windows)
	ifneq ($(FREE_WINDOWS),true)
	    plugin: $(DIR)/$(DEBUG_DIR)npB3DPlg$(SOEXT)
	endif
endif

$(DIR)/$(DEBUG_DIR)bin/blenderstatic: $(OBJS) $(GRPLIB) $(COMLIB) $(PULIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"static"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)bin/blenderstatic $(BUILDINFO_O) $(OBJS) $(GRPLIB) $(COMLIB) $(PULIB) $(PYLIB) $(LLIBS) $(SADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif

$(DIR)/$(DEBUG_DIR)bin/blender$(EXT): $(OBJS) $(GRPLIB) $(COMLIB) $(PULIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)bin/blender$(EXT) $(BUILDINFO_O) $(OBJS) $(GRPLIB) $(COMLIB) $(PULIB) $(PYLIB) $(LLIBS) $(DADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif

$(DIR)/$(DEBUG_DIR)bin/blenderplayer$(EXT): $(OBJS) $(SPLIB1) $(COMLIB) $(SPLIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)bin/blenderplayer$(EXT) $(BUILDINFO_O) $(OBJS) $(SPLIB1) $(COMLIB) $(SPLIB) $(PYPLAYERLIB) $(LLIBS) $(DADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif

$(DIR)/$(DEBUG_DIR)bin/blenderdynplayer$(EXT): $(OBJS) $(COMLIB) $(SPLIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)bin/blenderdynplayer$(EXT) $(BUILDINFO_O) $(OBJS) $(SPLIB1) $(COMLIB) $(SPLIB) $(PYLIB) $(LLIBS) $(DADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif

ifeq ($(OS),linux)
$(DIR)/$(DEBUG_DIR)bin/Blender3DPlugin$(SOEXT): $(PLUGAPPLIB_XPLINK)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(DYNLDFLAGS) -o $@ $(PLUGAPPLIB_XPLINK) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif
endif

# Windows build needs a def file? Why? For exposing functions from the
# dll to the outside world. However, this is alsi done internally..
DEFFILE = ./gameengine/GamePlayer/netscape/src/npB3DPlg.def

# Windows needs a specific name format for dll.s so we give it one.
ifeq ($(OS),windows)
$(DIR)/$(DEBUG_DIR)npB3DPlg$(SOEXT): $(NSPLUGLIB) $(PLUGAPPLIB) $(COMLIB) $(SPLIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
#	$(CCC) $(DYNLDFLAGS) -o $@ $(NSPLUGLIB) $(PLUGAPPLIB) $(COMLIB) $(SPLIB) $(PYLIB) $(PLUGREMLIB) $(LLIBS) $(DADD) $(LOPTS) 
	$(CCC) $(DYNLDFLAGS) -o $@ $(NSPLUGLIB) $(PLUGAPPLIB) $(COMLIB) $(SPLIB) $(PYLIB) $(PLUGREMLIB) $(LLIBS) $(DADD) $(LOPTS) /def:$(DEFFILE)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif
endif

ifeq ($(OS),$(findstring $(OS), "linux freebsd irix solaris"))
$(DIR)/$(DEBUG_DIR)bin/npBlender3DPlugin$(SOEXT): $(NSPLUGLIB) $(PLUGAPPLIB) $(COMLIB) $(SPLIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(DYNLDFLAGS) -o $@ $(NSPLUGLIB) $(PLUGAPPLIB) $(COMLIB) $(SPLIB) $(PYLIB) $(PLUGREMLIB) $(LLIBS) $(DADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif

$(DIR)/$(DEBUG_DIR)bin/npTestPlugin$(SOEXT): $(NSPLUGLIB)
	@echo "****> Link $@"
    ifdef NAN_BUILDINFO
	$(CCC) $(REL_CFLAGS) -DBUILD_DATE='"$(BUILD_DATE)"' -DBUILD_TIME='"$(BUILD_TIME)"' -DBUILD_PLATFORM='"$(CONFIG_GUESS)"' -DBUILD_TYPE='"dynamic"' $(BUILDINFO_C) -c -o $(BUILDINFO_O) -DNAN_BUILDINFO
    endif
	mkdir -p $(DIR)/$(DEBUG_DIR)bin
	$(CCC) $(DYNLDFLAGS) -o $@ $(NSPLUGLIB) $(PLUGTESTLIB) $(LLIBS) $(DADD) $(LOPTS)
    ifdef NAN_BUILDINFO
	/bin/rm $(BUILDINFO_O)
    endif
endif
