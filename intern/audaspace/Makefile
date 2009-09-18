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
#

include nan_definitions.mk

LIBNAME = audaspace
SOURCEDIR = intern/audaspace
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
DIRS += FX
DIRS += SDL
DIRS += SRC

ifeq ($(WITH_FFMPEG),true)
  DIRS += ffmpeg
endif

ifeq ($(WITH_OPENAL),true)
  DIRS += OpenAL
endif

ifeq ($(WITH_JACK),true)
  DIRS += jack
endif

ifeq ($(WITH_SNDFILE),true)
  DIRS += sndfile
endif

include nan_subdirs.mk

install: $(ALL_OR_DEBUG)
	@[ -d $(NAN_AUDASPACE) ] || mkdir $(NAN_AUDASPACE)
	@[ -d $(NAN_AUDASPACE)/include ] || mkdir $(NAN_AUDASPACE)/include
	@[ -d $(NAN_AUDASPACE)/lib/$(DEBUG_DIR) ] || mkdir $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libaudaspace.a $(DIR)/$(DEBUG_DIR)libaud_sdl.a $(DIR)/$(DEBUG_DIR)libaud_fx.a $(DIR)/$(DEBUG_DIR)libaud_src.a $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)

ifeq ($(WITH_FFMPEG),true)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libaud_ffmpeg.a $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)
endif

ifeq ($(WITH_OPENAL),true)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libaud_openal.a $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)
endif

ifeq ($(WITH_JACK),true)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libaud_jack.a $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)
endif

ifeq ($(WITH_SNDFILE),true)
	@../tools/cpifdiff.sh $(DIR)/$(DEBUG_DIR)libaud_sndfile.a $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)
endif

ifeq ($(OS),darwin)
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaudaspace.a
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_src.a
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_fx.a
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_sdl.a

ifeq ($(WITH_FFMPEG),true)
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_ffmpeg.a
endif

ifeq ($(WITH_OPENAL),true)
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_openal.a
endif

ifeq ($(WITH_JACK),true)
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_jack.a
endif

ifeq ($(WITH_SNDFILE),true)
	ranlib $(NAN_AUDASPACE)/lib/$(DEBUG_DIR)libaud_sndfile.a
endif

endif
	@../tools/cpifdiff.sh intern/*.h $(NAN_AUDASPACE)/include/
