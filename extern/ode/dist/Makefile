#########################################################################
#                                                                       #
# Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       #
# All rights reserved.  Email: russ@q12.org   Web: www.q12.org          #
#                                                                       #
# This library is free software; you can redistribute it and/or         #
# modify it under the terms of EITHER:                                  #
#   (1) The GNU Lesser General Public License as published by the Free  #
#       Software Foundation; either version 2.1 of the License, or (at  #
#       your option) any later version. The text of the GNU Lesser      #
#       General Public License is included with this library in the     #
#       file LICENSE.TXT.                                               #
#   (2) The BSD-style license that is included with this library in     #
#       the file LICENSE-BSD.TXT.                                       #
#                                                                       #
# This library is distributed in the hope that it will be useful,       #
# but WITHOUT ANY WARRANTY; without even the implied warranty of        #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    #
# LICENSE.TXT and LICENSE-BSD.TXT for more details.                     #
#                                                                       #
#########################################################################

USER_SETTINGS=config/user-settings
include $(USER_SETTINGS)
PLATFORM_MAKEFILE=config/makefile.$(PLATFORM)
include $(PLATFORM_MAKEFILE)

##############################################################################
# check some variables that were supposed to be defined

ifneq ($(BUILD),debug)
ifneq ($(BUILD),release)
$(error the BUILD variable is not set properly)
endif
endif

ifneq ($(PRECISION),SINGLE)
ifneq ($(PRECISION),DOUBLE)
$(error the PRECISION variable is not set properly)
endif
endif

##############################################################################
# package settings

ODE_SRC = \
	ode/src/array.cpp \
	ode/src/error.cpp \
	ode/src/memory.cpp \
	ode/src/obstack.cpp \
	ode/src/odemath.cpp \
	ode/src/matrix.cpp \
	ode/src/misc.cpp \
	ode/src/rotation.cpp \
	ode/src/mass.cpp \
	ode/src/ode.cpp \
	ode/src/step.cpp \
	ode/src/lcp.cpp \
	ode/src/joint.cpp \
	ode/src/space.cpp \
	ode/src/geom.cpp \
	ode/src/timer.cpp \
	ode/src/mat.cpp \
	ode/src/testing.cpp
ODE_PREGEN_SRC = \
	ode/src/fastldlt.c \
	ode/src/fastlsolve.c \
	ode/src/fastltsolve.c \
	ode/src/fastdot.c

ifeq ($(WINDOWS),1)
DRAWSTUFF_SRC = drawstuff/src/drawstuff.cpp drawstuff/src/windows.cpp
RESOURCE_FILE=lib/resources.RES
else
DRAWSTUFF_SRC = drawstuff/src/drawstuff.cpp drawstuff/src/x11.cpp
endif

ODE_LIB_NAME=ode
DRAWSTUFF_LIB_NAME=drawstuff

INCPATH=include
LIBPATH=lib

ODE_TEST_SRC_CPP = \
	ode/test/test_ode.cpp \
	ode/test/test_chain2.cpp \
	ode/test/test_hinge.cpp \
	ode/test/test_slider.cpp \
	ode/test/test_collision.cpp \
	ode/test/test_boxstack.cpp \
	ode/test/test_buggy.cpp \
	ode/test/test_joints.cpp \
	ode/test/test_space.cpp \
	ode/test/test_I.cpp \
	ode/test/test_step.cpp \
	ode/test/test_friction.cpp
ODE_TEST_SRC_C = \
	ode/test/test_chain1.c
DRAWSTUFF_TEST_SRC_CPP = \
	drawstuff/dstest/dstest.cpp

CONFIGURATOR_SRC=configurator.c
CONFIG_H=include/ode/config.h

##############################################################################
# derived things

DEFINES=

# add some defines depending on the build mode
ifeq ($(BUILD),release)
DEFINES+=$(C_DEF)dNODEBUG
endif
ifeq ($(BUILD),debug)
DEFINES+=$(C_DEF)dDEBUG_ALLOC
endif

# object file names
ODE_PREGEN_OBJECTS=$(ODE_PREGEN_SRC:%.c=%$(OBJ))
ODE_OBJECTS=$(ODE_SRC:%.cpp=%$(OBJ)) $(ODE_PREGEN_OBJECTS)
DRAWSTUFF_OBJECTS=$(DRAWSTUFF_SRC:%.cpp=%$(OBJ)) $(RESOURCE_FILE)

# side-effect variables causing creation of files containing lists of
# filenames to be linked, to work around command-line-length limitations
# on outdated 16-bit operating systems. because of command-line length
# limitations we cannot issue a link command with all object filenames
# specified (because this command is too long and overflows the command
# buffer), but instead must create a file containing all object filenames
# to be linked, and specify this list-file with @listfile on the command-line.
#
# the difficult part is doing this in a flexible way; we don't want to
# hard-code the to-be-linked object filenames in a file, but instead
# want to dynamically create a file containing a list of all object filenames
# within the $XXX_OBJECTS makefile variables. to do this, we use side-effect
# variables.
#
# idea: when these variables are EVALUATED (i.e. later during rule execution,
# not now during variable definition), they cause a SIDE EFFECT which creates
# a file with the list of all ODE object files. why the chicanery??? because
# if we have a command-line length limitation, no SINGLE command we issue will 
# be able to create a file containing all object files to be linked
# (because that command itself would need to include all filenames, making
# it too long to be executed). instead, we must use the gnu-make "foreach"
# function, combined - probably in an unintended way - with the "shell" 
# function. this is probably unintended because we are not using the "shell"
# function to return a string value for variable evaluation, but are instead 
# using the "shell" function to cause a side effect (appending of each filename
# to the filename-list-file).
#
# one possible snag is that, forbidding use of any external EXE utilities and
# relying only on the facilities provided by the outdated 16-bit operating
# system, there is no way to issue a SERIES of commands which append text to
# the end of a file WITHOUT adding newlines. therefore, the list of to-be-
# linked object files is separated by newlines in the list file. fortunately,
# the linker utility for this outdated 16-bit operating system accepts
# filenames on separate lines in the list file.

# remember: when we evaluate these variables later, this causes the creation
# of the appropriate list file.
ifeq ($(WINDOWS16),1)
SIDE_EFFECT_ODE_OBJLIST = $(foreach o,$(ODE_OBJECTS),$(shell echo $(o) >> odeobj.txt ))
SIDE_EFFECT_DRAWSTUFF_OBJLIST = $(foreach o,$(DRAWSTUFF_OBJECTS),$(shell echo $(o) >> dsobj.txt ))
endif

# library file names
ODE_LIB=$(LIBPATH)/$(LIB_PREFIX)$(ODE_LIB_NAME)$(LIB_SUFFIX)
DRAWSTUFF_LIB=$(LIBPATH)/$(LIB_PREFIX)$(DRAWSTUFF_LIB_NAME)$(LIB_SUFFIX)

# executable file names
ODE_TEST_EXE=$(ODE_TEST_SRC_CPP:%.cpp=%.exe) $(ODE_TEST_SRC_C:%.c=%.exe)
DRAWSTUFF_TEST_EXE=$(DRAWSTUFF_TEST_SRC_CPP:%.cpp=%.exe)
CONFIGURATOR_EXE=$(CONFIGURATOR_SRC:%.c=%.exe)

##############################################################################
# rules
#
# NOTE: the '.c' files are pregenerated sources, and must be compiled with
# -O1 optimization. that is why the rule for .c files is a bit different.
# why should it be compiled with O1? it is numerical code that is generated
# by fbuild. O1 optimization is used to preserve the operation orders that
# were discovered by fbuild to be the fastest on that platform. believe it or
# not, O2 makes this code run much slower for most compilers.

all: ode-lib drawstuff-lib ode-test drawstuff-test
	@echo SUCCESS

ode-lib: configure $(ODE_LIB)
drawstuff-lib: configure $(DRAWSTUFF_LIB)
ode-test: ode-lib drawstuff-lib $(ODE_TEST_EXE)
drawstuff-test: drawstuff-lib $(DRAWSTUFF_TEST_EXE)

ifndef ODE_LIB_AR_RULE
ODE_LIB_AR_RULE=$(AR)$@
endif

$(ODE_LIB): pre_ode_lib $(ODE_OBJECTS)
ifeq ($(WINDOWS16),1)
#   if we have a command-line-length limitation, then dynamically create
#   a file containing all object filenames, and pass this file to the linker
#   instead of directly specifying the object filenames on the command line.
#   the very evaluation of the following variable causes creation of file
#   odeobj.txt
	$(SIDE_EFFECT_ODE_OBJLIST)
	$(ODE_LIB_AR_RULE) @odeobj.txt
else
#   if we have no command-line-length limitation, directly specify all
#   object files to be linked.
	$(ODE_LIB_AR_RULE) $(ODE_OBJECTS)
endif

ifdef RANLIB
	$(RANLIB) $@
endif

$(DRAWSTUFF_LIB): pre_drawstuff_lib $(DRAWSTUFF_OBJECTS)
ifeq ($WINDOWS16),1)
#   if we have a command-line-length limitation, then do the same as above.
	$(SIDE_EFFECT_DRAWSTUFF_OBJLIST)
	$(AR)$@ @dsobj.txt
else
#   if we have no command-line-length limitation, directly specify all object
#   files to be linked.
	$(AR)$@ $(DRAWSTUFF_OBJECTS)
endif
ifdef RANLIB
	$(RANLIB) $@
endif

# rules to be executed before library linking starts: delete list file (if one is used)

pre_ode_lib:
ifeq ($WINDOWS16),1)
	$(DEL_CMD) odeobj.txt
endif

pre_drawstuff_lib:
ifeq ($WINDOWS16),1)
	$(DEL_CMD) dsobj.txt
endif

clean:
	-$(DEL_CMD) $(ODE_OBJECTS) $(ODE_TEST_EXE) $(ODE_LIB) $(DRAWSTUFF_OBJECTS) $(DRAWSTUFF_TEST_EXE) $(DRAWSTUFF_LIB) ode/test/*$(OBJ) drawstuff/dstest/*$(OBJ) $(CONFIGURATOR_EXE) $(CONFIG_H)

%$(OBJ): %.c
	$(CC) $(C_FLAGS) $(C_INC)$(INCPATH) $(DEFINES) $(C_OPT)1 $(C_OUT)$@ $<

%$(OBJ): %.cpp
	$(CC) $(C_FLAGS) $(C_INC)$(INCPATH) $(DEFINES) $(C_OPT)$(OPT) $(C_OUT)$@ $<

%.exe: %$(OBJ)
	$(CC) $(C_EXEOUT)$@ $< $(ODE_LIB) $(DRAWSTUFF_LIB) $(RESOURCE_FILE) $(LINK_OPENGL) $(LINK_MATH)


# windows specific rules

lib/resources.RES: drawstuff/src/resources.rc
	$(RC_RULE)


# configurator rules

configure: $(CONFIG_H)

$(CONFIG_H): $(CONFIGURATOR_EXE) $(USER_SETTINGS) $(PLATFORM_MAKEFILE)
	$(THIS_DIR)$(CONFIGURATOR_EXE) $(CONFIG_H) "$(CC) $(DEFINES) $(C_EXEOUT)" "$(DEL_CMD)" $(THIS_DIR)

$(CONFIGURATOR_EXE): $(CONFIGURATOR_SRC) $(USER_SETTINGS) $(PLATFORM_MAKEFILE)
	$(CC) $(C_DEF)d$(PRECISION) $(DEFINES) $(C_EXEOUT)$@ $<


# unix-gcc specific dependency making

DEP_RULE=gcc -M $(C_INC)$(INCPATH) $(DEFINES)
depend: $(ODE_SRC) $(ODE_PREGEN_SRC) $(DRAWSTUFF_SRC) $(ODE_TEST_SRC_CPP) $(ODE_TEST_SRC_C) $(DRAWSTUFF_TEST_SRC_CPP)
	$(DEP_RULE) $(ODE_SRC) $(ODE_PREGEN_SRC) | tools/process_deps ode/src/ > Makefile.deps
	$(DEP_RULE) $(DRAWSTUFF_SRC) | tools/process_deps drawstuff/src/ >> Makefile.deps
	$(DEP_RULE) $(ODE_TEST_SRC_CPP) | tools/process_deps ode/test/ >> Makefile.deps
	$(DEP_RULE) $(DRAWSTUFF_TEST_SRC_CPP) | tools/process_deps drawstuff/dstest/ >> Makefile.deps

include Makefile.deps
