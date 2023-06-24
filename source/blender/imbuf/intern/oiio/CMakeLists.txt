# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../blenkernel
  ../../../blenlib
  ../../../makesdna
  ../../../../../intern/guardedalloc
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
  ${PUGIXML_LIBRARIES}
)

if(WITH_IMAGE_OPENEXR)
  list(APPEND INC_SYS
    ${OPENEXR_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENEXR_LIBRARIES}
  )
endif()

blender_add_lib(bf_imbuf_openimageio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
