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

LIBNAME = blenloader
DIR = $(OCGDIR)/blender/$(LIBNAME)

include nan_compile.mk

# CFLAGS += $(LEVEL_2_C_WARNINGS)

# OpenGL and Python
CPPFLAGS += $(OGL_CPPFLAGS)
CPPFLAGS += -I$(NAN_PYTHON)/include/python$(NAN_PYTHON_VERSION)

# zlib
ifeq ($(OS),$(findstring $(OS), "solaris windows"))
    CPPFLAGS += -I$(NAN_ZLIB)/include
endif

# streaming write function
CPPFLAGS += -I../../writestreamglue
CPPFLAGS += -I../../readstreamglue

# initiate a streaming read pipe
CPPFLAGS += -I../../readblenfile

# This mod uses the GEN, DNA, BLI and BKE modules
CPPFLAGS += -I../../../kernel/gen_messaging
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../makesrna
CPPFLAGS += -I../../blenlib
# path to the guarded memory allocator
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../render/extern/include/
CPPFLAGS += -I../../python

# we still refer to /include a bit...
CPPFLAGS += -I../../editors/include

# path to our own external headerfiles
CPPFLAGS += -I..
