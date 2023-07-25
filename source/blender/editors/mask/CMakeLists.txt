# SPDX-FileCopyrightText: 2012 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../gpu
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  mask_add.cc
  mask_draw.cc
  mask_edit.cc
  mask_editaction.cc
  mask_ops.cc
  mask_query.cc
  mask_relationships.cc
  mask_select.cc
  mask_shapekey.cc

  mask_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_mask "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
