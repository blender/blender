# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../sequencer
  ../../windowmanager
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  area.cc
  area_query.c
  area_utils.c
  glutil.c
  screen_context.c
  screen_draw.c
  screen_edit.c
  screen_geometry.c
  screen_ops.c
  screen_user_menu.c
  screendump.c
  workspace_edit.c
  workspace_layout_edit.c
  workspace_listen.cc

  screen_intern.h
)

set(LIB
  bf_editor_datafiles
  bf_editor_space_sequencer
)


blender_add_lib(bf_editor_screen "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_screen bf_rna)
