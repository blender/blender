# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../common
  ../../blenloader
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/utfconv
  ../../../../extern/nanosvg
)

set(INC_SYS
)

set(SRC
  intern/gpencil_io_base.cc
  intern/gpencil_io_capi.cc
  intern/gpencil_io_import_base.cc
  intern/gpencil_io_import_svg.cc

  gpencil_io.h
  intern/gpencil_io_base.hh
  intern/gpencil_io_export_base.hh
  intern/gpencil_io_import_base.hh
  intern/gpencil_io_import_svg.hh

  # Only so this file is known by CMake.
  ../../../../extern/nanosvg/nanosvg.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
)

if(WITH_PUGIXML)
  list(APPEND SRC
    intern/gpencil_io_export_svg.cc

    intern/gpencil_io_export_svg.hh
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
    intern/gpencil_io_export_pdf.cc

    intern/gpencil_io_export_pdf.hh
  )
  list(APPEND INC_SYS
    ${HARU_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${HARU_LIBRARIES}

    # Haru needs `TIFFFaxBlackCodes` & `TIFFFaxWhiteCodes` symbols from TIFF.
    # Can be removed with Haru 2.4.0. They should be shipping with their own
    # Fax codes defined by default from that version onwards.
    ${TIFF_LIBRARY}
  )
  add_definitions(-DWITH_HARU)
endif()

if(WITH_BOOST)
  list(APPEND LIB
    ${BOOST_LIBRARIES}
  )
endif()

blender_add_lib(bf_gpencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
