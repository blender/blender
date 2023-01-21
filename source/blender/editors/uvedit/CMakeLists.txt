# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../geometry
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/eigen
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  uvedit_buttons.c
  uvedit_clipboard.cc
  uvedit_clipboard_graph_iso.cc
  uvedit_draw.c
  uvedit_islands.cc
  uvedit_ops.c
  uvedit_path.c
  uvedit_rip.c
  uvedit_select.c
  uvedit_smart_stitch.c
  uvedit_unwrap_ops.cc

  uvedit_clipboard_graph_iso.hh
  uvedit_intern.h
)

set(LIB
  bf_bmesh
)


blender_add_lib(bf_editor_uvedit "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_uvedit bf_rna)
