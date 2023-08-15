# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../makesrna
  ../../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  space_topbar.cc
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_space_topbar "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_topbar bf_rna)
