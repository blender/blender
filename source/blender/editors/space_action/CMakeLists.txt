# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../modifiers
  ../../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  action_buttons.cc
  action_data.cc
  action_draw.cc
  action_edit.cc
  action_ops.cc
  action_select.cc
  space_action.cc

  action_intern.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_space_action "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_action bf_rna)
