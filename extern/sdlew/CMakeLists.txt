# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  include
)

set(INC_SYS

)

set(SRC
  include/sdlew.h
  src/sdlew.c
)

set(LIB
)

blender_add_lib(extern_sdlew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
