# SPDX-FileCopyrightText: 2025 Blender Authors
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
  intern/attribute_set.cc
  intern/duplicate.cc
  intern/join.cc
  intern/point_cloud_edit.cc
  intern/point_cloud_ops.cc
  intern/point_cloud_selection.cc
  intern/point_cloud_undo.cc
  intern/separate.cc
  )

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::geometry
  PRIVATE bf::functions
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_point_cloud "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_point_cloud bf_rna)
