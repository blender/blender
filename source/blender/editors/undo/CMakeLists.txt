# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../asset_system
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
  ../../bmesh
)

set(INC_SYS
)

set(SRC
  ed_undo.cc
  memfile_undo.cc
  undo_system_types.cc

  undo_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
  bf_editor_curve
  bf_editor_curves
  bf_editor_lattice
  bf_editor_mesh
  bf_editor_metaball
  bf_editor_physics
)

blender_add_lib(bf_editor_undo "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
