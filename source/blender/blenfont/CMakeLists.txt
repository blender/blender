# SPDX-FileCopyrightText: 2008 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenkernel
  ../blentranslation
  ../gpu
  ../imbuf
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
  intern/blf_util.cc

  BLF_api.h
  intern/blf_internal.h
  intern/blf_internal_types.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_gpu
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


blender_add_lib(bf_blenfont "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
