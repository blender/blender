# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../asset
  ../include
  ../space_graph
  ../../makesrna
  ../../nodes
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ../../../../extern/fmtlib/include
)

set(SRC
  area.cc
  area_query.cc
  area_utils.cc
  glutil.cc
  screen_context.cc
  screen_draw.cc
  screen_edit.cc
  screen_geometry.cc
  screen_ops.cc
  screen_user_menu.cc
  screendump.cc
  workspace_edit.cc
  workspace_layout_edit.cc
  workspace_listen.cc

  screen_intern.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_datafiles
  bf_editor_space_sequencer
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
)

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
endif()

blender_add_lib(bf_editor_screen "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_screen bf_rna)
