# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenloader
  ../../bmesh
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../sequencer
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
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

  screen_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_datafiles
  bf_editor_space_sequencer
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_screen "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_screen bf_rna)
