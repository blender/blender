# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  ufbx.c
  ufbx.h
)

set(LIB
)

add_definitions(
  # Turn off ufbx features that Blender does not use
  -DUFBX_NO_SUBDIVISION
  -DUFBX_NO_TESSELLATION
  -DUFBX_NO_GEOMETRY_CACHE
  -DUFBX_NO_SCENE_EVALUATION
  -DUFBX_NO_SKINNING_EVALUATION
  -DUFBX_NO_ANIMATION_BAKING
  -DUFBX_NO_TRIANGULATION
  -DUFBX_NO_INDEX_GENERATION
  -DUFBX_NO_FORMAT_OBJ
)

blender_add_lib(extern_ufbx "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::extern::ufbx ALIAS extern_ufbx)
