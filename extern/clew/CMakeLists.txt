# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

set(INC
  .
  include
)

set(INC_SYS

)

set(SRC
  include/clew.h
  src/clew.c
)

set(LIB
)

add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_1_APIS)

blender_add_lib(extern_clew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
