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

LIBNAME = audaspace
DIR = $(OCGDIR)/intern/audaspace

include nan_compile.mk

CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

ifeq ($(WITH_SDL),true)
	CPPFLAGS += -DWITH_SDL
	CPPFLAGS += $(NAN_SDLCFLAGS)
endif

ifeq ($(WITH_OPENAL),true)
	CPPFLAGS += -DWITH_OPENAL
	CPPFLAGS += -I../OpenAL
endif

ifeq ($(WITH_JACK),true)
	CPPFLAGS += -DWITH_JACK
	CPPFLAGS += $(NAN_JACKCFLAGS)
	CPPFLAGS += -I../jack
endif

ifeq ($(WITH_FFMPEG),true)
	CPPFLAGS += -DWITH_FFMPEG
	CPPFLAGS += $(NAN_FFMPEGCFLAGS)
endif

ifeq ($(WITH_SNDFILE),true)
	CPPFLAGS += -DWITH_SNDFILE
	CPPFLAGS += -I../sndfile
endif

CPPFLAGS += -I$(LCGDIR)/samplerate/include/
CPPFLAGS += -I../ffmpeg
CPPFLAGS += -I../FX
CPPFLAGS += -I../SDL
CPPFLAGS += -I../SRC
CPPFLAGS += -I..
CPPFLAGS += -I.
