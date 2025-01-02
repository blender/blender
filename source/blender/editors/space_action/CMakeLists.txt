# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../modifiers

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
  PRIVATE bf::blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::animrig
  PRIVATE bf::rna
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_space_action "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
