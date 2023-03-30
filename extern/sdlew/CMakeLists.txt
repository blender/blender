# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

set(INC
  .
  include
)

set(INC_SYS
 ${X11_X11_INCLUDE_PATH}
)

set(SRC
  include/sdlew.h
  src/sdlew.c
)

set(LIB
)

blender_add_lib(extern_sdlew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
