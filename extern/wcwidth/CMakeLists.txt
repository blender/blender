# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS

)

set(SRC
  wcwidth.c

  wcwidth.h
)

set(LIB
)

blender_add_lib(extern_wcwidth "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
