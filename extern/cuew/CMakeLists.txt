# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

set(INC
  .
  include
)

set(INC_SYS

)

set(SRC
  src/cuew.c

  include/cuew.h
)

set(LIB
)

blender_add_lib(extern_cuew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
