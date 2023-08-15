# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../asset
  ../../asset_system
  ../../blenfont
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/mantaflow/extern

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  drawobject.cc
  space_view3d.cc
  view3d_buttons.cc
  view3d_camera_control.cc
  view3d_cursor_snap.cc
  view3d_draw.cc
  view3d_edit.cc
  view3d_gizmo_armature.cc
  view3d_gizmo_camera.cc
  view3d_gizmo_empty.cc
  view3d_gizmo_forcefield.cc
  view3d_gizmo_light.cc
  view3d_gizmo_navigate.cc
  view3d_gizmo_navigate_type.cc
  view3d_gizmo_preselect.cc
  view3d_gizmo_preselect_type.cc
  view3d_gizmo_ruler.cc
  view3d_gizmo_tool_generic.cc
  view3d_header.cc
  view3d_iterators.cc
  view3d_navigate.cc
  view3d_navigate_fly.cc
  view3d_navigate_smoothview.cc
  view3d_navigate_view_all.cc
  view3d_navigate_view_axis.cc
  view3d_navigate_view_camera.cc
  view3d_navigate_view_center_cursor.cc
  view3d_navigate_view_center_pick.cc
  view3d_navigate_view_dolly.cc
  view3d_navigate_view_move.cc
  view3d_navigate_view_ndof.cc
  view3d_navigate_view_orbit.cc
  view3d_navigate_view_roll.cc
  view3d_navigate_view_rotate.cc
  view3d_navigate_view_pan.cc
  view3d_navigate_view_zoom.cc
  view3d_navigate_walk.cc
  view3d_navigate_zoom_border.cc
  view3d_ops.cc
  view3d_placement.cc
  view3d_project.cc
  view3d_select.cc
  view3d_snap.cc
  view3d_utils.cc
  view3d_view.cc

  view3d_intern.h
  view3d_navigate.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_curves
  bf_editor_lattice
  bf_editor_mesh
  PRIVATE bf::intern::guardedalloc
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

# RNA_prototypes.h
add_dependencies(bf_editor_space_view3d bf_rna)
