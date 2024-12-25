# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../makesrna
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  annotate_draw.cc
  annotate_paint.cc
  editaction_gpencil.cc
  gpencil_data.cc
  gpencil_edit.cc
  gpencil_ops.cc
  gpencil_undo.cc
  gpencil_utils.cc

  gpencil_intern.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_gpencil_legacy "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_gpencil_legacy bf_rna)
