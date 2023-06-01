# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../depsgraph
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  editmball_undo.c
  mball_edit.c
  mball_ops.c

  mball_intern.h
)

set(LIB
)

blender_add_lib(bf_editor_metaball "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
