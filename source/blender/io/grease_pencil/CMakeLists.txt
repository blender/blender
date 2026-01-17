# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../common
  ../../editors/include
  ../../makesdna
  ../../makesrna
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
)

set(SRC
  intern/grease_pencil_io.cc
  intern/grease_pencil_io_import_svg.cc

  grease_pencil_io.hh
  intern/grease_pencil_io_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::extern::nanosvg
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::dependencies::optional::pugixml
  PRIVATE bf::dependencies::optional::haru
  bf_io_common
)

if(WITH_PUGIXML)
  list(APPEND SRC
    intern/grease_pencil_io_export_svg.cc
  )
endif()

if(WITH_HARU)
  list(APPEND SRC
    intern/grease_pencil_io_export_pdf.cc
  )
endif()

blender_add_lib(bf_io_grease_pencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
