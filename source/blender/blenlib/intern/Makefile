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

LIBNAME = blenlib
DIR = $(OCGDIR)/blender/$(LIBNAME)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# path to SDNA types
CPPFLAGS += -I../../makesdna
# path to our own external headerfiles
CPPFLAGS += -I..
# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
# path to freetype font stuff
CPPFLAGS += -I$(NAN_FREETYPE)/include
CPPFLAGS += -I$(NAN_FREETYPE)/include/freetype2
# path to blenkernel
CPPFLAGS += -I$(SRCHOME)/blender/blenkernel
CPPFLAGS += -I../../editors/include/
# path to zlib
CPPFLAGS += -I$(NAN_ZLIB)/include

CPPFLAGS += -I../../gpu

CPPFLAGS += -I$(NAN_GHOST)/include

ifdef NAN_PTHREADS
  CPPFLAGS += -I$(NAN_PTHREADS)/include
endif
ifeq ($(OS),linux)
	CPPFLAGS += -I$(OCGDIR)/extern/binreloc/include
endif
