# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../depsgraph
  ../../functions
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/atomic
  ../../../../intern/guardedalloc
  ../../bmesh
)

set(INC_SYS
)

set(SRC
  geometry_attributes.cc
  geometry_ops.cc

  geometry_intern.hh
)

set(LIB
  bf_blenkernel
  bf_depsgraph
  bf_editor_object
  bf_windowmanager
)

blender_add_lib(bf_editor_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
