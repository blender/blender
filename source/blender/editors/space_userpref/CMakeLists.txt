# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../makesrna
  ../../windowmanager

)

set(INC_SYS

)

set(SRC
  space_userpref.c
  userpref_ops.c

  userpref_intern.h
)

set(LIB
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_space_userpref "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

