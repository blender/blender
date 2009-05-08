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
# GHOST gears test application Makefile
#

DIR = $(OCGDIR)/intern/ghost/test

# we don't want a library here, only object files:
ALLTARGETS = $(OBJS)

include nan_compile.mk
include nan_link.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)
CCFLAGS += $(LEVEL_1_CPP_WARNINGS)

CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_STRING)/include
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../..

OCGGHOST = $(OCGDIR)/intern/ghost

LIBS = $(OCGGHOST)/$(DEBUG_DIR)libghost.a
SLIBS += $(LCGDIR)/string/lib/libstring.a
SLIBS += $(LCGDIR)/guardedalloc/lib/libguardedalloc.a

all::
	@echo "- link $(DIR)/$(DEBUG_DIR)multitest -"
	@$(CCC) $(LDFLAGS) -o $(DIR)/$(DEBUG_DIR)multitest $(OBJS) $(LIBS) $(SLIBS) $(LLIBS) $(DADD) $(LOPTS)
