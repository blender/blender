# SPDX-FileCopyrightText: 2021 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  include
)

set(INC_SYS

)

set(SRC
  src/hipew.c
  include/hipew.h
  src/util.h
)

set(LIB
)

blender_add_lib(extern_hipew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
