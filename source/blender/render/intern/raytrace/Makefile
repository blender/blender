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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

LIBNAME = render_raytrace
DIR = $(OCGDIR)/blender/render

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

# first /include is my own includes, second is the external interface. 
# The external modules follow after. There should be a nicer way to say this.
CPPFLAGS += -I../include 
CPPFLAGS += -I../../extern/include 
CPPFLAGS += -I../../../blenlib
CPPFLAGS += -I../../../imbuf
CPPFLAGS += -I../../../makesdna
CPPFLAGS += -I../../../makesrna
CPPFLAGS += -I../../../blenkernel
CPPFLAGS += -I../../../quicktime
CPPFLAGS += -I../../../../kernel/gen_messaging
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
# not very neat: the rest of blender..
CPPFLAGS += -I../../../editors/include 
CPPFLAGS += $(NAN_SDLCFLAGS)
CPPFLAGS += -I../../../../../intern/smoke/extern

ifeq ($(WITH_QUICKTIME), true)
    CPPFLAGS += -DWITH_QUICKTIME
endif

ifeq ($(WITH_FFMPEG),true)
    CPPFLAGS += -DWITH_FFMPEG
endif

ifeq ($(WITH_OPENEXR),true)
    CPPFLAGS += -DWITH_OPENEXR
endif
