# SPDX-FileCopyrightText: 2008 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../makesrna
)

set(INC_SYS
  ${FREETYPE_INCLUDE_DIRS}
)

set(SRC
  intern/blf.cc
  intern/blf_default.cc
  intern/blf_dir.cc
  intern/blf_font.cc
  intern/blf_font_default.cc
  intern/blf_glyph.cc
  intern/blf_thumbs.cc
  BLF_api.hh
  intern/blf_internal.hh
  intern/blf_internal_types.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  ${FREETYPE_LIBRARIES} ${BROTLI_LIBRARIES}
)

if(WIN32)
  list(APPEND SRC
    intern/blf_font_win32_compat.cc
  )
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
else()
  # SVG icons.
  list(APPEND LIB
    PRIVATE bf::extern::nanosvg
    PRIVATE bf::editor::datafiles
  )

endif()

blender_add_lib(bf_blenfont "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blenfont ALIAS bf_blenfont)
