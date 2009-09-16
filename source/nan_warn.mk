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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): GSR
#
# ***** END GPL LICENSE BLOCK *****
#
# NaN compiler and linker warning levels
# On some platforms, you will be flooded with system include file warnings.
# Use hmake to filter those away.
#

# Force the correct redefinition
LEVEL_1_C_WARNINGS = -FIX_NAN_WARN
LEVEL_1_CPP_WARNINGS = -FIX_NAN_WARN
LEVEL_2_C_WARNINGS = -FIX_NAN_WARN
LEVEL_2_CPP_WARNINGS = -FIX_NAN_WARN
FIX_STUBS_WARNINGS = -FIX_NAN_WARN

########################################################################
# Level 1: basic C warnings.
ifeq ($(CC),gcc)
    LEVEL_1_C_WARNINGS = -Wall
    LEVEL_1_C_WARNINGS += -Wno-char-subscripts
else
  ifeq ($(CC),cc)
    ifeq ($(OS),irix)
      # MIPSpro Compilers
      #
      # Irix warning info
      #
      # 1001 # the source file does not end w/ a newline
      # 1110 # unreachable statement
      # 1201 # trailing comma in enums is nonstandard
      # 1209 # constant controlling expressions
      # 1355 # extra semicolon is ignored
      # 1424 # unreferenced template paramaters
      # 1681 # virtual function override
      # 3201 # unreferenced formal paramaters
      #
      LEVEL_1_C_WARNINGS = -fullwarn -woff 1001,1110,1201,1209,1355,1424,1681,3201
    endif
  endif
  ifeq ($(OS),windows)
    # Microsoft Compilers and cl_wrapper.pl
    LEVEL_1_C_WARNINGS = -Wall
  endif
endif

# Level 1: basic CPP warnings.
ifeq ($(CCC),g++)
    LEVEL_1_CPP_WARNINGS = -Wall
    LEVEL_1_CPP_WARNINGS += -Wno-reorder
else
  ifeq ($(CCC),CC)
    ifeq ($(OS),irix)
      # MIPSpro Compilers
      #  see warning descriptions above
      LEVEL_1_CPP_WARNINGS = -woff 1001,1110,1201,1209,1355,1424,1681,3201
    endif
  endif
  ifeq ($(OS),windows)
    # Microsoft Compilers and cl_wrapper.pl
    LEVEL_1_CPP_WARNINGS = -Wall
  endif
endif

########################################################################
# Level 2: paranoia level C warnings.
# DO NOT REUSE LEVEL_1_ DEFINES.
ifeq ($(CC),gcc)
    LEVEL_2_C_WARNINGS = -Wall
    LEVEL_2_C_WARNINGS += -W
    # deliberately enable char-subscript warnings
    LEVEL_2_C_WARNINGS += -Wshadow
    LEVEL_2_C_WARNINGS += -Wpointer-arith
    LEVEL_2_C_WARNINGS += -Wbad-function-cast
    LEVEL_2_C_WARNINGS += -Wcast-qual
    LEVEL_2_C_WARNINGS += -Wcast-align
    LEVEL_2_C_WARNINGS += -Waggregate-return
    LEVEL_2_C_WARNINGS += -Wstrict-prototypes
    LEVEL_2_C_WARNINGS += -Wmissing-prototypes
    LEVEL_2_C_WARNINGS += -Wmissing-declarations
    LEVEL_2_C_WARNINGS += -Wnested-externs
    LEVEL_2_C_WARNINGS += -Wredundant-decls 
else
  ifeq ($(CC),cc)
    ifeq ($(OS),irix)
      # MIPSpro Compilers
      #  see warning descriptions above
      LEVEL_2_C_WARNINGS = -fullwarn -woff 1001,1209,1424,3201
    endif
    ifeq ($(OS),solaris)
      # Forte / Sun WorkShop Compilers
      LEVEL_2_C_WARNINGS = -v
    endif
  endif
  ifeq ($(OS),windows)
    # Microsoft Compilers and cl_wrapper.pl
    LEVEL_2_C_WARNINGS = -Wall
  endif
endif

# Level 2: paranoia level CPP warnings.
# DO NOT REUSE LEVEL_1_ DEFINES.
ifeq ($(CCC),g++)
    LEVEL_2_CPP_WARNINGS = -Wall
    LEVEL_2_CPP_WARNINGS += -W
    # deliberately enable char-subscript warnings
    LEVEL_2_CPP_WARNINGS += -Wshadow
    LEVEL_2_CPP_WARNINGS += -Wpointer-arith
    LEVEL_2_CPP_WARNINGS += -Wcast-qual
    LEVEL_2_CPP_WARNINGS += -Wcast-align
    # deliberately disable aggregate-return warnings
    LEVEL_2_CPP_WARNINGS += -Wredundant-decls 
    LEVEL_2_CPP_WARNINGS += -Wreorder
    LEVEL_2_CPP_WARNINGS += -Wctor-dtor-privacy
    LEVEL_2_CPP_WARNINGS += -Wnon-virtual-dtor
    #LEVEL_2_CPP_WARNINGS += -Wold-style-cast
    LEVEL_2_CPP_WARNINGS += -Woverloaded-virtual
    LEVEL_2_CPP_WARNINGS += -Wsign-promo
    LEVEL_2_CPP_WARNINGS += -Wsynth
else
  ifeq ($(CCC),CC)
    ifeq ($(OS),irix)
      # MIPSpro Compilers
      #  see warning descriptions above
      LEVEL_2_CPP_WARNINGS = -fullwarn -woff 1209,1424,3201
    endif
  endif
  ifeq ($(OS),windows)
    # Microsoft Compilers and cl_wrapper.pl
    LEVEL_2_CPP_WARNINGS = -Wall
  endif
endif

########################################################################
# stubs warning fix
ifeq ($(CC),gcc)
    FIX_STUBS_WARNINGS = -Wno-unused
else
    FIX_STUBS_WARNINGS =
endif

