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

include nan_definitions.mk

LIBNAME = SoundSystem
SOURCEDIR = intern/SoundSystem
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
DIRS += dummy

ifneq ($(NAN_NO_OPENAL),true)
  ifeq ($(OS),windows)
      DIRS += fmod 
      DIRS += openal sdl
  endif
  ifeq ($(OS),darwin)
      DIRS += openal
  endif
  ifeq ($(OS),$(findstring $(OS), "linux freebsd solaris"))
    DIRS += openal sdl
  endif
else
  export CPPFLAGS += -DNO_SOUND
endif

include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_SOUNDSYSTEM) ] || mkdir $(NAN_SOUNDSYSTEM)
	@[ -d $(NAN_SOUNDSYSTEM)/include ] || mkdir $(NAN_SOUNDSYSTEM)/include
	@[ -d $(NAN_SOUNDSYSTEM)/lib ] || mkdir $(NAN_SOUNDSYSTEM)/lib
	@[ -d $(NAN_SOUNDSYSTEM)/lib/debug ] || mkdir $(NAN_SOUNDSYSTEM)/lib/debug
	@../tools/cpifdiff.sh $(DIR)/libSoundSystem.a $(NAN_SOUNDSYSTEM)/lib/
	@../tools/cpifdiff.sh $(DIR)/debug/libSoundSystem.a $(NAN_SOUNDSYSTEM)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_SOUNDSYSTEM)/lib/libSoundSystem.a
	ranlib $(NAN_SOUNDSYSTEM)/lib/debug/libSoundSystem.a
endif
	@../tools/cpifdiff.sh *.h $(NAN_SOUNDSYSTEM)/include/


