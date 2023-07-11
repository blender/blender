# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../depsgraph
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/clog
)

set(INC_SYS

)

set(SRC
  editlattice_select.cc
  editlattice_tools.cc
  editlattice_undo.cc
  lattice_ops.cc

  lattice_intern.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_lattice "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
