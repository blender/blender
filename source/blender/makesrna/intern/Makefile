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

ALLRNA = $(wildcard rna_*.c)
DEFRNA = $(filter-out %rna_define.c, $(filter-out %rna_access.c, $(ALLRNA)))

GENSRCS = $(patsubst rna_%.c, rna_%_gen.c, $(DEFRNA))
GENTARGET = $(patsubst %.c, $(DIR)/$(DEBUG_DIR)%.c, $(GENSRCS))

MAKESRCS = $(DEFRNA) makesrna.c rna_define.c $(wildcard ../../editors/*/*_api.c)
MAKEOBJS = $(patsubst %.c, $(DIR)/$(DEBUG_DIR)%.o, $(notdir $(MAKESRCS)))

CSRCS = $(GENSRCS) rna_access.c

include nan_compile.mk

ifdef NAN_DEPEND
-include $(MAKESRCS:%.c=$(DIR)/$(DEBUG_DIR)%.d)
endif

CFLAGS += $(LEVEL_1_C_WARNINGS)

CPPFLAGS += -I$(NAN_GUARDEDALLOC)/include
CPPFLAGS += -I../../blenlib
CPPFLAGS += -I../../blenkernel
CPPFLAGS += -I../../makesdna
CPPFLAGS += -I../../windowmanager
CPPFLAGS += -I../../editors/include
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
	@$(RM) $(DIR)/makesrna* $(DIR)/rna*
	@$(RM) $(DIR)/debug/makesrna* $(DIR)/debug/rna*

# TODO include right .mk for ldflags

# XXX this is an ugly hack, copying code from nan_compile.mk
# we want the .o's to be in the makesrna/ directory, but the
# .c's are in the editors/*/ directories

$(DIR)/$(DEBUG_DIR)%_api.o: ../../editors/*/%_api.c
    ifdef NAN_DEPEND
	@set -e; $(CC) -M $(CPPFLAGS) $< 2>/dev/null \
		| sed 's@\($*\)\.o[ :]*@$(DIR)/$(DEBUG_DIR)\1.o : @g' \
		> $(DIR)/$(DEBUG_DIR)$*.d; \
		[ -s $(DIR)/$(DEBUG_DIR)$*.d ] || $(RM) $(DIR)/$*.d
    endif
    ifdef NAN_QUIET
	@echo " -- $< -- "
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    else
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@)
    endif

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

