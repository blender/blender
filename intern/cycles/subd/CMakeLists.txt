# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS

)

set(SRC
  dice.cpp
  interpolation.cpp
  osd.cpp
  patch.cpp
  split.cpp
)

set(SRC_HEADERS
  dice.h
  interpolation.h
  osd.h
  patch.h
  split.h
  subpatch.h
)

set(LIB

)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_subd "${LIB}" ${SRC} ${SRC_HEADERS})
