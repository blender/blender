# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
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
# Contributor(s): GSR, Stefan Gartner
#
# ***** END GPL LICENSE BLOCK *****
#
# Compile and archive

include nan_definitions.mk

CPPFLAGS ?= $(NAN_CPPFLAGS)

# common parts ---------------------------------------------------

# Uncomment next lines to enable integrated game engine
ifneq ($(NAN_NO_KETSJI), true)
    CFLAGS  += -DGAMEBLENDER=1
    ifeq ($(NAN_USE_BULLET), true)
      CFLAGS  += -DUSE_BULLET
      CCFLAGS += -DUSE_BULLET
    endif
else
   CPPFLAGS += -DNO_KETSJI
endif

ifeq ($(BF_PROFILE), true)
    CFLAGS += -pg
    CCFLAGS += -pg
endif

ifeq ($(WITH_BF_OPENMP), true)
    CFLAGS += -fopenmp
    CCFLAGS += -fopenmp
endif

ifdef NAN_DEBUG
    CFLAGS += $(NAN_DEBUG)
    CCFLAGS += $(NAN_DEBUG)
endif

REL_CFLAGS  += -DNDEBUG
REL_CCFLAGS += -DNDEBUG
DBG_CFLAGS  += -g
DBG_CCFLAGS += -g

# OS dependent parts ---------------------------------------------------

ifeq ($(OS),darwin)
    CC ?= gcc
    CCC ?= g++
    ifeq ($(MACOSX_DEPLOYMENT_TARGET), 10.4)
        CC = gcc-4.0
        CCC = g++-4.0
    else
        ifeq ($(MACOSX_DEPLOYMENT_TARGET), 10.5)
            CC  = gcc-4.2
            CCC = g++-4.2
        endif
    endif
    ifeq ($(CPU),powerpc)
        CFLAGS  += -pipe -fPIC -mcpu=7450 -mtune=G5 -funsigned-char -fno-strict-aliasing
        CCFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    else
        CFLAGS  += -pipe -fPIC -funsigned-char
        CCFLAGS += -pipe -fPIC -funsigned-char
    endif


    CFLAGS += -arch $(MACOSX_ARCHITECTURE) #-isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MACOSX_MIN_VERS)
    CCFLAGS += -arch $(MACOSX_ARCHITECTURE) #-isysroot $(MACOSX_SDK) -mmacosx-version-min=$(MACOSX_MIN_VERS)

    ifeq ($(MACOSX_ARCHITECTURE), $(findstring $(MACOSX_ARCHITECTURE), "i386 x86_64"))
        REL_CFLAGS += -O2 -ftree-vectorize -msse -msse2 -msse3
        REL_CCFLAGS += -O2 -ftree-vectorize -msse -msse2 -msse3
    else
        REL_CFLAGS += -O2
        REL_CCFLAGS += -O2
    endif

    CPPFLAGS += -D_THREAD_SAFE -fpascal-strings

    ifeq ($(WITH_COCOA), true)
        CPPFLAGS += -DGHOST_COCOA
    endif
    ifeq ($(USE_QTKIT), true)
        CPPFLAGS += -DUSE_QTKIT
    endif

    NAN_DEPEND  = true
    OPENGL_HEADERS = /System/Library/Frameworks/OpenGL.framework
    AR = ar
    ARFLAGS = ruv
    RANLIB = ranlib
    ARFLAGSQUIET = ru
endif

ifeq ($(OS),freebsd)
    CC  ?= gcc
    CCC ?= g++
    JAVAC = javac
    JAVAH = javah
    CFLAGS  += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    CCFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    REL_CFLAGS  += -O2
    REL_CCFLAGS += -O2
    CPPFLAGS += -D_THREAD_SAFE
    NAN_DEPEND = true
    OPENGL_HEADERS  = /usr/X11R6/include
    JAVA_HEADERS = /usr/local/jdk1.3.1/include
    JAVA_SYSTEM_HEADERS = /usr/local/jdk1.3.1/include/freebsd
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
endif

ifeq ($(OS),irix)
    ifeq ($(IRIX_USE_GCC),true)
        CC  ?= gcc
        CCC ?= g++
        CFLAGS += -fPIC -funsigned-char -fno-strict-aliasing -mabi=n32 -mips4
        CCFLAGS += -fPIC -fpermissive -funsigned-char -fno-strict-aliasing -mabi=n32 -mips4
        REL_CFLAGS += -O2
        REL_CCFLAGS += -O2 
        DBG_CFLAGS += -g3 -gdwarf-2 -ggdb
        DBG_CCFLAGS += -g3 -gdwarf-2 -ggdb
    else
        CC  ?= cc
        CCC ?= CC
        CFLAGS  += -n32 -mips3 -Xcpluscomm
        CCFLAGS += -n32 -mips3 -Xcpluscomm -LANG:std
        ifdef MIPS73_ISOHEADERS
            CCFLAGS += -LANG:libc_in_namespace_std=off -I$(MIPS73_ISOHEADERS)
        else
            CCFLAGS += -LANG:libc_in_namespace_std=off
        endif
        REL_CFLAGS  += -n32 -mips3 -O2 -OPT:Olimit=0
        REL_CCFLAGS += -n32 -mips3 -O2 -OPT:Olimit=0
    endif
    OPENGL_HEADERS = /usr/include
    NAN_DEPEND = true
    AR = CC
    ARFLAGS = -ar -o
    ARFLAGSQUIET = -ar -o
endif

ifeq ($(OS),linux)
    CC  ?= gcc
    CCC ?= g++
#    CFLAGS += -pipe
#    CCFLAGS += -pipe
    CFLAGS  += -pipe -fPIC -funsigned-char -fno-strict-aliasing -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
    CCFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
    REL_CFLAGS  += -O2
    REL_CCFLAGS += -O2
    NAN_DEPEND = true
  ifeq ($(CPU),alpha)
    CFLAGS += -mieee
  endif
    OPENGL_HEADERS = /usr/X11R6/include
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
endif

ifeq ($(OS),openbsd)
    CC  ?= gcc
    CCC ?= g++
    CFLAGS  += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    CCFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    REL_CFLAGS  += -O2
    REL_CCFLAGS += -O2
    NAN_DEPEND = true
    CPPFLAGS += -D__FreeBSD__
    OPENGL_HEADERS = /usr/X11R6/include
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
endif

ifeq ($(OS),solaris)
    # Adding gcc flag to $CC is not good, however if its not there makesdna wont build - Campbell
    ifeq (x86_64, $(findstring x86_64, $(CPU)))
        CC  ?= gcc -m64
        CCC ?= g++ -m64
    else
        CC  ?= gcc
        CCC ?= g++
        #CC  ?= cc
        #CCC ?= CC
    endif

    JAVAC = javac
    JAVAH = javah
    CFLAGS  += -pipe -fPIC -funsigned-char -fno-strict-aliasing
    CCFLAGS += -pipe -fPIC -funsigned-char -fno-strict-aliasing
#    CFLAGS  += "-fast -xdepend -xarch=v8plus -xO3 -xlibmil -KPIC -DPIC -xchar=unsigned"
#    CCFLAGS += "-fast -xdepend -xarch=v8plus -xO3 -xlibmil -xlibmopt -features=tmplife -norunpath -KPIC -DPIC -xchar=unsigned"

    # Note, you might still want to compile a 32 bit binary if you have a 64bit system. if so remove the following lines
#    ifeq ($(findstring 64,$(CPU)), 64)
#        CFLAGS  += -m64
#        CCFLAGS += -m64
#    endif

    REL_CFLAGS  += -O2
    REL_CCFLAGS += -O2

    NAN_DEPEND = true
#    ifeq ($(CPU),sparc)
    ifeq ($(findstring sparc,$(CPU)), sparc)
        OPENGL_HEADERS = /usr/openwin/share/include
        CPPFLAGS += -DSUN_OGL_NO_VERTEX_MACROS
        JAVA_HEADERS = /usr/java/include
        JAVA_SYSTEM_HEADERS = /usr/java/include/solaris
    else
        # OPENGL_HEADERS = /usr/X11/include/mesa
        OPENGL_HEADERS = /usr/X11/include/
    endif
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
endif

ifeq ($(OS),windows)
  ifeq ($(FREE_WINDOWS),true)
    CC  ?= gcc
    CCC ?= g++
    CFLAGS += -pipe -mno-cygwin -mwindows -funsigned-char -fno-strict-aliasing
    CCFLAGS += -pipe -mno-cygwin -mwindows -funsigned-char -fno-strict-aliasing
    CPPFLAGS += -DFREE_WINDOWS
    REL_CFLAGS += -O2
    REL_CCFLAGS += -O2
    NAN_DEPEND = true
    #OPENGL_HEADERS = /usr/include/w32api
    OPENGL_HEADERS = ./
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
    WINRC = $(wildcard *.rc)
    RANLIB = ranlib
  else
    CC  ?= $(SRCHOME)/tools/cygwin/cl_wrapper.pl
    CCC ?= $(SRCHOME)/tools/cygwin/cl_wrapper.pl
    JAVAC = $(SRCHOME)/tools/cygwin/java_wrapper.pl -c
    JAVAH = $(SRCHOME)/tools/cygwin/java_wrapper.pl -h
    REL_CFLAGS  += /O2
    REL_CCFLAGS += /O2 -GX
    DBG_CFLAGS  += /Fd$(DIR)/debug/
    DBG_CCFLAGS += /Fd$(DIR)/debug/
    CFLAGS += /MT
    CCFLAGS += /MT
    NAN_DEPEND = true
    OPENGL_HEADERS = .
    CPPFLAGS += -DWIN32 -D_WIN32 -D__WIN32
    CPPFLAGS += -D_M_IX86
    CPPFLAGS += -I"/cygdrive/c/Program Files/Microsoft Visual Studio/VC98/include"
    JAVA_HEADERS = /cygdrive/c/j2sdk1.4.0-beta3/include
    JAVA_SYSTEM_HEADERS = /cygdrive/c/j2sdk1.4.0-beta3/include/win32
    CPP = $(SRCHOME)/tools/cygwin/cl_wrapper.pl
    AR = ar
    ARFLAGS = ruv
    ARFLAGSQUIET = ru
    WINRC = $(wildcard *.rc)
  endif
endif

ifeq (debug, $(findstring debug, $(MAKECMDGOALS)))
    export DEBUG_DIR=debug/
endif

ifneq (x$(DEBUG_DIR), x)
    CFLAGS +=$(DBG_CFLAGS)
    CCFLAGS+=$(DBG_CCFLAGS)
else
    CFLAGS +=$(REL_CFLAGS)
    CCFLAGS+=$(REL_CCFLAGS)
endif

# Note: include nan_warn's LEVEL_*_WARNINGS after CC/OS have been set.
include nan_warn.mk

# compile rules

default: all

$(DIR)/$(DEBUG_DIR)%.o: %.c
    ifdef NAN_DEPEND
	@set -e; $(CC) -M $(CPPFLAGS) $< 2>/dev/null \
		| sed 's@\($*\)\.o[ :]*@$(DIR)/$(DEBUG_DIR)\1.o : @g' \
		> $(DIR)/$(DEBUG_DIR)$*.d; \
		[ -s $(DIR)/$(DEBUG_DIR)$*.d ] || $(RM) $(DIR)/$(DEBUG_DIR)$*.d
    endif
    ifdef NAN_QUIET
	@echo " -- $< -- "
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    else
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    endif

$(DIR)/$(DEBUG_DIR)%.o: %.cpp
    ifdef NAN_DEPEND
	@set -e; $(CCC) -M $(CPPFLAGS) $< 2>/dev/null \
		| sed 's@\($*\)\.o[ :]*@$(DIR)/$(DEBUG_DIR)\1.o : @g' \
		> $(DIR)/$(DEBUG_DIR)$*.d; \
		[ -s $(DIR)/$(DEBUG_DIR)$*.d ] || $(RM) $(DIR)/$(DEBUG_DIR)$*.d
    endif
    ifdef NAN_QUIET
	@echo " -- $< -- "
	@$(CCC) -c $(CCFLAGS) $(CPPFLAGS) $< -o $@
    else
	$(CCC) -c $(CCFLAGS) $(CPPFLAGS) $< -o $@
    endif

$(DIR)/$(DEBUG_DIR)%.o: %.mm
    ifdef NAN_DEPEND
	@set -e; $(CC) -M $(CPPFLAGS) $< 2>/dev/null \
		| sed 's@\($*\)\.o[ :]*@$(DIR)/$(DEBUG_DIR)\1.o : @g' \
		> $(DIR)/$(DEBUG_DIR)$*.d; \
		[ -s $(DIR)/$(DEBUG_DIR)$*.d ] || $(RM) $(DIR)/$(DEBUG_DIR)$*.d
    endif
    ifdef NAN_QUIET
	@echo " -- $< -- "
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    else
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    endif

$(DIR)/$(DEBUG_DIR)%.o: %.m
    ifdef NAN_DEPEND
	@set -e; $(CC) -M $(CPPFLAGS) $< 2>/dev/null \
		| sed 's@\($*\)\.o[ :]*@$(DIR)/$(DEBUG_DIR)\1.o : @g' \
		> $(DIR)/$(DEBUG_DIR)$*.d; \
		[ -s $(DIR)/$(DEBUG_DIR)$*.d ] || $(RM) $(DIR)/$(DEBUG_DIR)$*.d
    endif
    ifdef NAN_QUIET
	@echo " -- $< -- "
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    else
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
    endif


$(DIR)/$(DEBUG_DIR)%.res: %.rc
ifeq ($(FREE_WINDOWS),true)
	windres $< -O coff -o $@
else
	$(SRCHOME)/tools/cygwin/cl_wrapper.pl - rc /fo$@ $<
endif

$(DIR)/$(DEBUG_DIR)%.class: %.java
    ifdef JARS
	$(JAVAC) -verbose -g -deprecation -sourcepath . -classpath "$(JARS)" -d $(DIR)/$(DEBUG_DIR) $<
    else
	$(JAVAC) -verbose -g -deprecation -d $(DIR)/$(DEBUG_DIR) $<
    endif

$(DIR)/$(DEBUG_DIR)%.h: $(DIR)/$(DEBUG_DIR)%.class
	$(JAVAH) -classpath $(DIR)/$(DEBUG_DIR) -d $(DIR)/$(DEBUG_DIR) -jni $*

%.h:
	@echo "WARNING: Fake header creation rule used, dependencies will be remade"

CSRCS  ?= $(wildcard *.c)
CCSRCS ?= $(wildcard *.cpp)
JSRCS  ?= $(wildcard *.java)

ifdef NAN_DEPEND
-include $(CSRCS:%.c=$(DIR)/$(DEBUG_DIR)%.d) $(CCSRCS:%.cpp=$(DIR)/$(DEBUG_DIR)%.d) $(OCCSRCS:$.mm=$(DIR)/$(DEBUG_DIR)%.d) $(OCSRCS:$.m=$(DIR)/$(DEBUG_DIR)%.d)
endif

OBJS_AR := $(OBJS)
OBJS_AR += $(CSRCS:%.c=%.o)
OBJS_AR += $(CCSRCS:%.cpp=%.o)
OBJS_AR += $(OCCSRCS:%.mm=%.o)
OBJS_AR += $(OCSRCS:%.m=%.o)
OBJS_AR += $(WINRC:%.rc=%.res)

OBJS += $(CSRCS:%.c=$(DIR)/$(DEBUG_DIR)%.o)
OBJS += $(CCSRCS:%.cpp=$(DIR)/$(DEBUG_DIR)%.o)
OBJS += $(OCCSRCS:%.mm=$(DIR)/$(DEBUG_DIR)%.o)
OBJS += $(OCSRCS:%.m=$(DIR)/$(DEBUG_DIR)%.o)
OBJS += $(WINRC:%.rc=$(DIR)/$(DEBUG_DIR)%.res)

JCLASS += $(JSRCS:%.java=$(DIR)/$(DEBUG_DIR)%.class)

LIB_a = $(DIR)/$(DEBUG_DIR)lib$(LIBNAME).a

$(LIB_a): $(OBJS)
   # $OBJS can be empty except for some spaces
    ifneq (x, x$(strip $(OBJS)))
      ifdef NAN_PARANOID
	@$(RM) $(LIB_a)
        ifdef NAN_QUIET
	@echo " -- lib: lib$(LIBNAME).a -- "
	@cd $(DIR)/$(DEBUG_DIR); $(AR) $(ARFLAGSQUIET) $@ $(OBJS_AR)
        else
	cd $(DIR)/$(DEBUG_DIR); $(AR) $(ARFLAGS) $@ $(OBJS_AR)
        endif
      else
        ifdef NAN_QUIET
	@echo " -- lib: lib$(LIBNAME).a -- "
	@$(AR) $(ARFLAGSQUIET) $@ $?
        else
	$(AR) $(ARFLAGS) $@ $?
        endif
      endif
      ifdef RANLIB
	$(RANLIB) $@
      endif
    endif

ALLTARGETS ?= $(LIB_a)

all debug :: makedir $(ALLTARGETS)

lib: $(LIB_a)

creator: $(OBJS)
	@echo "====> make creator subtarget in `pwd | sed 's/^.*develop\///'`"
	@$(MAKE) makedir DIR=$(DIR)/$(DEBUG_DIR)cre
	@$(MAKE) lib CSRCS="$(CRE_CSRCS)" LIBNAME=$(LIBNAME)$@

publisher: $(OBJS)
	@echo "====> make publisher subtarget in `pwd | sed 's/^.*develop\///'`"
	@$(MAKE) makedir DIR=$(DIR)/$(DEBUG_DIR)pub
	@$(MAKE) lib CSRCS="$(PUB_CSRCS)" LIBNAME=$(LIBNAME)$@

player: $(OBJS)
	@echo "====> make player subtarget in `pwd | sed 's/^.*develop\///'`"
	@$(MAKE) makedir DIR=$(DIR)/player/$(DEBUG_DIR)
	@$(MAKE) lib CSRCS="$(SAP_CSRCS)" LIBNAME=$(LIBNAME)$@

clean:: optclean debugclean

optclean::
	@-[ ! -d $(DIR) ] || ( cd $(DIR) && \
	    $(RM) *.o *.a *.d *.res ii_files/*.ii *.class *.h )

debugclean::
	@-[ ! -d $(DIR)/debug ] || ( cd $(DIR)/debug && \
	    $(RM) *.o *.a *.d *.res ii_files/*.ii *.class *.h )

.PHONY: makedir
makedir::
	@# don't use mkdir -p. Cygwin will try to make network paths and fail
	@[ -d $(DIR) ] || mkdir $(DIR)
	@[ -d $(DIR)/debug ] || mkdir $(DIR)/debug

