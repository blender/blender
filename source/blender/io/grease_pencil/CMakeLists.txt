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
  ../../../../extern/fmtlib/include
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
  PRIVATE extern_fmtlib
  bf_io_common
)

if(WITH_PUGIXML)
  list(APPEND SRC
    intern/grease_pencil_io_export_svg.cc
  )
  list(APPEND INC_SYS
    ${PUGIXML_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${PUGIXML_LIBRARIES}
  )
  add_definitions(-DWITH_PUGIXML)
endif()

if(WITH_HARU)
  list(APPEND SRC
    intern/grease_pencil_io_export_pdf.cc
  )
  list(APPEND INC_SYS
    ${HARU_INCLUDE_DIRS}
  )
  list(APPEND LIB
    PRIVATE ${HARU_LIBRARIES}
    PRIVATE ${TIFF_LIBRARY} # needed for TIFFFaxBlackCodes in older (<5.0) lib folders
  )
  add_definitions(-DWITH_HARU)
endif()

blender_add_lib(bf_io_grease_pencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
