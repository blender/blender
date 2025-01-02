# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../../../intern/eigen
)

set(INC_SYS
)

set(SRC
  uvedit_buttons.cc
  uvedit_clipboard.cc
  uvedit_clipboard_graph_iso.cc
  uvedit_draw.cc
  uvedit_islands.cc
  uvedit_ops.cc
  uvedit_path.cc
  uvedit_rip.cc
  uvedit_select.cc
  uvedit_smart_stitch.cc
  uvedit_unwrap_ops.cc

  uvedit_clipboard_graph_iso.hh
  uvedit_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::rna
  PRIVATE bf::windowmanager
)

if(WITH_UV_SLIM)
  list(APPEND LIB
    bf_intern_slim
  )
  add_definitions(-DWITH_UV_SLIM)
endif()


blender_add_lib(bf_editor_uvedit "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
