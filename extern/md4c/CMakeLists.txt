# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  md4c.c
  md4c.h
)

set(LIB
)

blender_add_lib(extern_md4c "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::md4c ALIAS extern_md4c)
