# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../makesrna
  ../../windowmanager

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS

)

set(SRC
  space_userpref.cc
  userpref_ops.cc

  userpref_intern.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::guardedalloc
  bf_windowmanager
)

blender_add_lib(bf_editor_space_userpref "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_space_userpref bf_rna)
