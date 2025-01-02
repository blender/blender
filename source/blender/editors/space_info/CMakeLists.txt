# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include

)

set(INC_SYS
)

set(SRC
  info_draw.cc
  info_ops.cc
  info_report.cc
  info_stats.cc
  space_info.cc
  textview.cc

  info_intern.hh
  textview.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::rna
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_editor_space_info "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
