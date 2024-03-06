# SPDX-FileCopyrightText: 2013 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../blenkernel
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

# Keep until APPLE/ARM libraries are updated.
if(APPLE OR WIN32)
  if(CMAKE_OSX_ARCHITECTURES MATCHES arm64 OR CMAKE_SYSTEM_PROCESSOR MATCHES ARM64)
    list(APPEND INC_SYS
      ${IMATH_INCLUDE_DIRS}
    )
  endif()
endif()

blender_add_lib(bf_imbuf_openimageio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
