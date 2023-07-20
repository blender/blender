# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blentranslation
  ../../bmesh
  ../../geometry
  ../../gpu
  ../../makesrna
  ../../../../intern/eigen
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  uvedit_buttons.cc
  uvedit_clipboard.cc
  uvedit_clipboard_graph_iso.cc
  uvedit_draw.cc
  uvedit_islands.cc
  uvedit_ops.cc
  uvedit_path.cc
  uvedit_rip.cc
  uvedit_select.cc
  uvedit_smart_stitch.cc
  uvedit_unwrap_ops.cc

  uvedit_clipboard_graph_iso.hh
  uvedit_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  bf_bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_uvedit "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_uvedit bf_rna)
