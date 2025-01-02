# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
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

  curve_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::animrig
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::gpu
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::rna
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_curve "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
