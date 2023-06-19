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
  patch.cpp
  split.cpp
  patch_table.cpp
)

set(SRC_HEADERS
  dice.h
  patch.h
  patch_table.h
  split.h
  subpatch.h
)

set(LIB

)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_subd "${LIB}" ${SRC} ${SRC_HEADERS})
