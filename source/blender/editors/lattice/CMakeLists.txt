# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../makesrna
)

set(INC_SYS

)

set(SRC
  editlattice_select.cc
  editlattice_tools.cc
  editlattice_undo.cc
  lattice_ops.cc

  lattice_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_editor_lattice "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
