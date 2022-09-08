# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(SRC
  console_draw.c
  console_ops.c
  space_console.c

  console_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_console "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
