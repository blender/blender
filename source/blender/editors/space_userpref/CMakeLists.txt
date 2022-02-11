# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  space_userpref.c
  userpref_ops.c

  userpref_intern.h
)

set(LIB
)

blender_add_lib(bf_editor_space_userpref "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
