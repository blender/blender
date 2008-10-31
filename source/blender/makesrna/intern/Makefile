#
# $Id: Makefile 13161 2008-01-07 19:13:47Z hos $
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
# Contributor(s): Blender Foundation (2008).
#
# ***** END GPL LICENSE BLOCK *****
#

LIBNAME = rna
DIR = $(OCGDIR)/blender/makesrna
CSRCS = RNA.c $(wildcard rna_*.c)

include nan_compile.mk

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I..
CPPFLAGS += -I.

ifeq ($(OS),windows)
    # Windows needs these extra libs because of winstuff... It is not 
    # _really_ needed, but it is the easiest fix for now. If you have 
    # some spare time, try to trace down the exact dep.
	ifneq ($(FREE_WINDOWS),true)
		WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib
		WINLIBS +=	advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
		WINLIBS +=	winmm.lib opengl32.lib glu32.lib largeint.lib
		WINLIBS +=	/link /nodefaultlib:libc
	else
		LDFLAGS += -mwindows -mno-cygwin
	endif
endif

clean::
	@$(RM) $(DIR)/makesrna* $(DIR)/RNA.c
	@$(RM) $(DIR)/debug/makesrna* $(DIR)/debug/RNA.c

# TODO include right .mk for ldflags

# A small note: we do not use the debug version of the alloc lib. That
# is done quite intentionally. If there is a bug in that lib, it needs
# to be fixed by the module maintainer.

RNAOBJS = $(filter-out %RNA.o, $(OBJS))

$(DIR)/$(DEBUG_DIR)makesrna: $(RNAOBJS) $(DIR)/$(DEBUG_DIR)makesrna.o $(OCGDIR)/blender/makesdna/$(DEBUG_DIR)libdna.a $(NAN_GUARDEDALLOC)/lib/libguardedalloc.a
	$(CC) $(LDFLAGS) -o $@ $(WINLIBS) $^

$(DIR)/$(DEBUG_DIR)RNA.c: $(DIR)/$(DEBUG_DIR)makesrna
    ifeq ($(OS),windows)
	$(SRCHOME)/tools/cygwin/cl_wrapper.pl - $(DIR)/$(DEBUG_DIR)makesrna \
	    $(DIR)/$(DEBUG_DIR)RNA.c
    else
	$(DIR)/$(DEBUG_DIR)makesrna $(DIR)/$(DEBUG_DIR)RNA.c
    endif

$(DIR)/$(DEBUG_DIR)makesrna.o: makesrna.c $(wildcard rna_*.c)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) makesrna.c -o $@

