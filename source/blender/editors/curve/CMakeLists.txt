# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../extern/curve_fit_nd
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  curve_ops.c
  editcurve.c
  editcurve_add.c
  editcurve_paint.c
  editcurve_pen.c
  editcurve_query.c
  editcurve_select.c
  editcurve_undo.cc
  editfont.c
  editfont_undo.c

  curve_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  extern_curve_fit_nd
)


blender_add_lib(bf_editor_curve "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_curve bf_rna)
