# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../functions
  ../../geometry
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(SRC
  intern/curves_add.cc
  intern/curves_ops.cc
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

blender_add_lib(bf_editor_curves "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_editor_curves bf_rna)
