# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../makesrna
  ../../windowmanager
)

set(INC_SYS
)

set(SRC
  ed_id_management.cc
)

set(LIB
  bf_blenkernel
  bf_windowmanager
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_editor_id_management "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
