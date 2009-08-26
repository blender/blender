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
# The Original Code is Copyright (C) 2002 by Hans Lambermont
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****
# Bounces make to subdirectories. Also installs after succesful all target.

include nan_definitions.mk

SOURCEDIR = extern
DIR = $(OCGDIR)/extern
DIRS = glew/src

ifeq ($(WITH_FFMPEG), true)
ifeq ($(NAN_FFMPEG), $(LCGDIR)/ffmpeg)
    DIRS += ffmpeg
endif
ifeq ($(NAN_FFMPEG), $(LCGDIR)/gcc/ffmpeg)
    DIRS += ffmpeg
endif
endif

# Cloth requires it
#ifneq ($(NAN_NO_KETSJI), true)
DIRS += bullet2
#endif

ifeq ($(WITH_BINRELOC), true)
    DIRS += binreloc
endif

ifeq ($(WITH_OPENJPEG), true)
    DIRS += libopenjpeg
endif

ifeq ($(WITH_LZO), true)
    DIRS += lzo/minilzo
endif

ifeq ($(WITH_LZMA), true)
    DIRS += lzma
endif

TARGET = solid

all::
	@[ -d $(OCGDIR)/extern ] || mkdir -p $(OCGDIR)/extern
	@for i in $(DIRS); do \
	    echo "====> $(MAKE) $@ in $(SOURCEDIR)/$$i" ;\
	    $(MAKE) -C $$i install || exit 1; \
	done

clean test debug::
	@[ -d $(OCGDIR)/extern ] || mkdir -p $(OCGDIR)/extern
	@for i in $(DIRS); do \
	    echo "====> $(MAKE) $@ in $(SOURCEDIR)/$$i" ;\
	    $(MAKE) -C $$i $@ || exit 1; \
	done
