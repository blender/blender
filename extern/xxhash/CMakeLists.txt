# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  xxhash.c
  xxhash.h
)

set(LIB
)

blender_add_lib(extern_xxhash "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::xxhash ALIAS extern_xxhash)
