# SPDX-FileCopyrightText: 2006-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/grease_pencil_add.cc
  intern/grease_pencil_edit.cc
  intern/grease_pencil_frames.cc
  intern/grease_pencil_layers.cc
  intern/grease_pencil_ops.cc
  intern/grease_pencil_select.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_grease_pencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_curves bf_rna)
