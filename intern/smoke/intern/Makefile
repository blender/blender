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
# smoke intern Makefile
#

LIBNAME = smoke
DIR = $(OCGDIR)/intern/$(LIBNAME)

include nan_compile.mk

unexport NAN_QUIET

CCFLAGS += $(LEVEL_2_CPP_WARNINGS)

ifeq ($(WITH_BF_OPENMP),true)
   CPPFLAGS += -DPARALLEL
endif

ifeq ($(WITH_FFTW3),true)
    CPPFLAGS += -DFFTW3=1
    CPPFLAGS += $(BF_FFTW3_INC)
endif

CPPFLAGS += -I.
CPPFLAGS += -I../extern
CPPFLAGS += -I$(NAN_PNG)/include
CPPFLAGS += -I$(NAN_PNG)/include/libpng

# zlib
ifeq ($(OS),$(findstring $(OS), "solaris windows"))
    CPPFLAGS += -I$(NAN_ZLIB)/include
endif
