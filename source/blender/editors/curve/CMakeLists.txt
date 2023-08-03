# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blentranslation
  ../../depsgraph
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  curve_ops.cc
  editcurve.cc
  editcurve_add.cc
  editcurve_paint.cc
  editcurve_pen.cc
  editcurve_query.cc
  editcurve_select.cc
  editcurve_undo.cc
  editfont.cc
  editfont_undo.cc

  curve_intern.h
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_editor_curve "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_curve bf_rna)
