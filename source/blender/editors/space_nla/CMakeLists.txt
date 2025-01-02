# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include

)

set(INC_SYS
)

set(SRC
  nla_buttons.cc
  nla_draw.cc
  nla_edit.cc
  nla_ops.cc
  nla_select.cc
  nla_tracks.cc
  space_nla.cc

  nla_intern.hh
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
  PRIVATE bf::rna
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_space_nla "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
