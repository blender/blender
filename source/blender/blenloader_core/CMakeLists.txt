# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  intern/blo_core_bhead.cc
  intern/blo_core_blend_header.cc

  BLO_core_bhead.hh
  BLO_core_blend_header.hh
)

set(LIB
  PRIVATE bf::blenlib
)

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

blender_add_lib(bf_blenloader_core "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blenloader_core ALIAS bf_blenloader_core)
