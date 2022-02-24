# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

if(CMAKE_COMPILER_IS_GNUCC)
  remove_cc_flag(
    "-Wshadow"
    "-Werror=shadow"
  )
endif()

set(INC
  .
)

set(INC_SYS

)

set(SRC
  mikktspace.c

  mikktspace.h
)

set(LIB
)

blender_add_lib(bf_intern_mikktspace "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
