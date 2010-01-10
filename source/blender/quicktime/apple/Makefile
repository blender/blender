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

LIBNAME = blenderqt
DIR = $(OCGDIR)/blender/$(LIBNAME)

ifeq ($(OS), $(findstring $(OS), "darwin"))
    ifeq ($(USE_QTKIT),true)
        OCSRCS += $(wildcard *.m)
    endif
endif

include nan_compile.mk

CFLAGS += $(LEVEL1_C_WARNINGS)

CPPFLAGS += -DWITH_QUICKTIME

ifeq ($(CPU),$(findstring $(CPU), "powerpc mips sparc"))
    CPPFLAGS += -DWORDS_BIGENDIAN
else
    # alpha i386
    CPPFLAGS += -DWORDS_LITTLEENDIAN
endif

# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
# our own include
CPPFLAGS += -I..

# stuff needed by quicktime_[import|export].c
CPPFLAGS += -I../../blenloader -I../../imbuf/intern -I../../imbuf 
CPPFLAGS += -I../../blenlib -I../../makesdna -I../../editors/include -I../../avi 
CPPFLAGS += -I../../blenkernel -I../../render/extern/include -I../../windowmanager -I../../makesrna

