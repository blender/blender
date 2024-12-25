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
  space_statusbar.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_space_statusbar "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh dna_type_offsets.h
add_dependencies(bf_editor_space_statusbar bf_rna)
