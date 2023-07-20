# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../../../intern/clog
  ../../../../intern/eigen
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  gizmo_draw_utils.cc
  gizmo_geometry.h
  gizmo_library_intern.h
  gizmo_library_presets.cc
  gizmo_library_utils.cc
  geometry/geom_arrow_gizmo.cc
  geometry/geom_cube_gizmo.cc
  geometry/geom_dial_gizmo.cc
  gizmo_types/arrow3d_gizmo.cc
  gizmo_types/blank3d_gizmo.cc
  gizmo_types/button2d_gizmo.cc
  gizmo_types/cage2d_gizmo.cc
  gizmo_types/cage3d_gizmo.cc
  gizmo_types/dial3d_gizmo.cc
  gizmo_types/move3d_gizmo.cc
  gizmo_types/primitive3d_gizmo.cc
  gizmo_types/snap3d_gizmo.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_gizmo_library "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_gizmo_library bf_rna)
