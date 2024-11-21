# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../windowmanager
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
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_gpencil_legacy "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_gpencil_legacy bf_rna)
