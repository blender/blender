# SPDX-FileCopyrightText: 2012 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
