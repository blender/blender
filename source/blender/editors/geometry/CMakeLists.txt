# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../asset_system
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../functions
  ../../makesrna
  ../../nodes
  ../../windowmanager

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  geometry_attributes.cc
  geometry_ops.cc
  geometry_randomization.cc
  node_group_operator.cc

  geometry_intern.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_object
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::guardedalloc
  bf_windowmanager
)

blender_add_lib(bf_editor_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh dna_type_offsets.h
add_dependencies(bf_editor_geometry bf_rna)
