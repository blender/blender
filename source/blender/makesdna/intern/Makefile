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

LIBNAME = dna
DIR = $(OCGDIR)/blender/makesdna
CSRCS = DNA.c $(wildcard dna_*.c)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I$(OPENGL_HEADERS)
CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I..

ifeq ($(OS),windows)
    # Windows needs these extra libs because of winstuff... It is not 
    # _really_ needed, but it is the easiest fix for now. If you have 
    # some spare time, try to trace down the exact dep. Then again, you 
    # could also spend that time making the sdna system more robust.
	ifneq ($(FREE_WINDOWS),true)
		WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
		WINLIBS +=	advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
		WINLIBS +=	winmm.lib opengl32.lib glu32.lib largeint.lib
		WINLIBS +=	/link /nodefaultlib:libc
	else
		LDFLAGS += -mwindows -mno-cygwin
	endif
endif

ifeq ($(OS), darwin)
    LDFLAGS += -arch $(MACOSX_ARCHITECTURE) #-isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MACOSX_MIN_VERS)
endif

clean::
	@$(RM) $(DIR)/makesdna* $(DIR)/DNA.c
	@$(RM) $(DIR)/debug/makesdna* $(DIR)/debug/DNA.c

# TODO include right .mk for ldflags

# A small note: we do not use the debug version of the alloc lib. That
# is done quite intentionally. If there is a bug in that lib, it needs
# to be fixed by the module maintainer.
DNAOBJS = $(filter-out %DNA.o, $(OBJS))

$(DIR)/$(DEBUG_DIR)makesdna: $(DNAOBJS) $(DIR)/$(DEBUG_DIR)makesdna.o $(NAN_GUARDEDALLOC)/lib/libguardedalloc.a
	$(CC) $(LDFLAGS) -o $@ $^

$(DIR)/$(DEBUG_DIR)DNA.c: $(DIR)/$(DEBUG_DIR)makesdna
    ifeq ($(OS),windows)
	$(SRCHOME)/tools/cygwin/cl_wrapper.pl - $(DIR)/$(DEBUG_DIR)makesdna \
	    $(DIR)/$(DEBUG_DIR)DNA.c
    else
	$(DIR)/$(DEBUG_DIR)makesdna $(DIR)/$(DEBUG_DIR)DNA.c
    endif

$(DIR)/$(DEBUG_DIR)makesdna.o: makesdna.c $(wildcard ../*.h)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) makesdna.c -o $@

