# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../makesrna
  ../../windowmanager

)

set(INC_SYS

)

set(SRC
  space_userpref.cc
  userpref_ops.cc

  userpref_intern.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_space_userpref "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
