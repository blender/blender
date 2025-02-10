# SPDX-FileCopyrightText: 2013 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../../../intern/utfconv
)

set(INC_SYS
  ${OPENIMAGEIO_INCLUDE_DIRS}
)

set(SRC
  openimageio_api.h
  openimageio_support.hh

  openimageio_api.cpp
  openimageio_support.cc
)

set(LIB
  ${OPENIMAGEIO_LIBRARIES}
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PUGIXML)
  list(APPEND LIB
    ${PUGIXML_LIBRARIES}
  )
endif()

if(WITH_IMAGE_OPENEXR)
  list(APPEND INC_SYS
    ${OPENEXR_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENEXR_LIBRARIES}
  )
endif()

# OpenImageIO headers include `Imath` headers when there is no SSE support for
# matrix operations. This depends on the specific architecture and compiler
# flags, most reliable is to always include the `Imath` headers if we have them.
if(DEFINED IMATH_INCLUDE_DIRS)
  list(APPEND INC_SYS
    ${IMATH_INCLUDE_DIRS}
  )
endif()

blender_add_lib(bf_imbuf_openimageio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
