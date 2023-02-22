# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2012 Blender Foundation

set(INC
  .
)

set(INC_SYS
  ${X11_X11_INCLUDE_PATH}
)

set(SRC
  xdnd.c
  xdnd.h
)

set(LIB
)

add_definitions(
  -DHAVE_SYS_TIME_H
)

blender_add_lib(extern_xdnd "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
