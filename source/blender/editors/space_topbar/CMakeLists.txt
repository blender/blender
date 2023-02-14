# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  space_topbar.c
)

set(LIB
)


blender_add_lib(bf_editor_space_topbar "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


# RNA_prototypes.h dna_type_offsets.h
add_dependencies(bf_editor_space_topbar bf_rna)
