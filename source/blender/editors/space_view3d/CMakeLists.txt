# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../asset
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../intern/mantaflow/extern

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  drawobject.cc
  space_view3d.cc
  view3d_buttons.c
  view3d_camera_control.c
  view3d_cursor_snap.c
  view3d_draw.cc
  view3d_edit.c
  view3d_gizmo_armature.c
  view3d_gizmo_camera.c
  view3d_gizmo_empty.c
  view3d_gizmo_forcefield.c
  view3d_gizmo_light.c
  view3d_gizmo_navigate.c
  view3d_gizmo_navigate_type.c
  view3d_gizmo_preselect.c
  view3d_gizmo_preselect_type.cc
  view3d_gizmo_ruler.cc
  view3d_gizmo_tool_generic.c
  view3d_header.c
  view3d_iterators.cc
  view3d_navigate.cc
  view3d_navigate_dolly.c
  view3d_navigate_fly.c
  view3d_navigate_move.c
  view3d_navigate_ndof.c
  view3d_navigate_roll.c
  view3d_navigate_rotate.c
  view3d_navigate_smoothview.c
  view3d_navigate_walk.c
  view3d_navigate_zoom.c
  view3d_navigate_zoom_border.c
  view3d_ops.c
  view3d_placement.c
  view3d_project.c
  view3d_select.cc
  view3d_snap.c
  view3d_utils.c
  view3d_view.c

  view3d_intern.h
  view3d_navigate.h
)

set(LIB
  bf_editor_curves
  bf_editor_lattice
  bf_editor_mesh
)

if(WITH_PYTHON)
  list(APPEND INC ../../python)
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

blender_add_lib(bf_editor_space_view3d "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_editor_space_view3d bf_dna)
# RNA_prototypes.h
add_dependencies(bf_editor_space_view3d bf_rna)
