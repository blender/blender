# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2008 Blender Foundation

set(INC
  include
)

set(INC_SYS

)

set(SRC
  binreloc.c

  include/binreloc.h
)

set(LIB
)

add_definitions(-DENABLE_BINRELOC)


blender_add_lib(extern_binreloc "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
