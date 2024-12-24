# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  ed_id_management.cc
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::windowmanager
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_id_management "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
