# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc

)

set(INC_SYS
)

set(SRC
  console_draw.cc
  console_ops.cc
  space_console.cc

  console_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_console "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

