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
# Contributor(s): Blender Foundation (2008).
#
# ***** END GPL LICENSE BLOCK *****
#

LIBNAME = rna
DIR = $(OCGDIR)/blender/makesrna

ALLRNA = $(wildcard rna_*.c)
DEFRNA = $(filter-out %rna_define.c, $(filter-out %rna_access.c, $(ALLRNA)))

GENRNA = $(filter-out %_api.c, $(DEFRNA))
GENSRCS = $(patsubst rna_%.c, rna_%_gen.c, $(GENRNA))
GENTARGET = $(patsubst %.c, $(DIR)/$(DEBUG_DIR)%.c, $(GENSRCS))

MAKESRCS = $(DEFRNA) makesrna.c rna_define.c
MAKEOBJS = $(patsubst %.c, $(DIR)/$(DEBUG_DIR)%.o, $(notdir $(MAKESRCS)))

CSRCS = $(GENSRCS) rna_access.c

include nan_compile.mk

ifdef NAN_DEPEND
-include $(MAKESRCS:%.c=$(DIR)/$(DEBUG_DIR)%.d)
endif

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../../../intern/audaspace/intern
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../imbuf
CPPFLAGS += -I../../ikplugin
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../windowmanager
CPPFLAGS += -I../../editors/include
CPPFLAGS += -I../../gpu
CPPFLAGS += -I../../render/extern/include
CPPFLAGS += -I..
CPPFLAGS += -I.

ifeq ($(WITH_FFMPEG),true)
    CPPFLAGS += -DWITH_FFMPEG
    CPPFLAGS += $(NAN_FFMPEGCFLAGS)
endif

ifeq ($(WITH_OPENEXR), true)
    CPPFLAGS += -DWITH_OPENEXR
endif

ifeq ($(WITH_DDS), true)
    CPPFLAGS += -DWITH_DDS
endif

ifeq ($(WITH_OPENJPEG), true)
    CPPFLAGS += -DWITH_OPENJPEG
endif

ifeq ($(WITH_QUICKTIME), true)
    CPPFLAGS += -DWITH_QUICKTIME -I../../quicktime
endif

ifeq ($(WITH_SDL),true)
	CPPFLAGS += -DWITH_SDL
endif

ifeq ($(WITH_JACK),true)
	CPPFLAGS += -DWITH_JACK
endif

ifeq ($(WITH_OPENAL),true)
	CPPFLAGS += -DWITH_OPENAL
endif

ifeq ($(WITH_TIFF),true)
    CPPFLAGS += -DWITH_TIFF
endif

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

ifeq ($(OS), darwin)
    LDFLAGS += -arch $(MACOSX_ARCHITECTURE) #-isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MACOSX_MIN_VERS)
endif

clean::
	@$(RM) $(DIR)/makesrna* $(DIR)/rna*
	@$(RM) $(DIR)/debug/makesrna* $(DIR)/debug/rna*

# TODO include right .mk for ldflags

# A small note: we do not use the debug version of the alloc lib. That
# is done quite intentionally. If there is a bug in that lib, it needs
# to be fixed by the module maintainer.

$(DIR)/$(DEBUG_DIR)makesrna: $(MAKEOBJS) $(OCGDIR)/blender/makesdna/$(DEBUG_DIR)libdna.a $(NAN_GUARDEDALLOC)/lib/libguardedalloc.a
	$(CC) $(LDFLAGS) -o $@ $(WINLIBS) $^

$(DIR)/$(DEBUG_DIR)rna_phony_gen.c: $(DIR)/$(DEBUG_DIR)makesrna
    ifeq ($(OS),windows)
	$(SRCHOME)/tools/cygwin/cl_wrapper.pl - $(DIR)/$(DEBUG_DIR)makesrna \
	    $(DIR)/$(DEBUG_DIR)
    else
	$(DIR)/$(DEBUG_DIR)makesrna $(DIR)/$(DEBUG_DIR)
    endif
	@touch $@

$(GENTARGET): $(DIR)/$(DEBUG_DIR)rna_phony_gen.c

$(DIR)/$(DEBUG_DIR)makesrna.o: makesrna.c $(wildcard rna_*.c)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) makesrna.c -o $@

