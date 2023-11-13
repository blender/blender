# SPDX-FileCopyrightText: 2006 Blender Authors
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

)

set(SRC
  openexr_api.cpp
  openexr_api.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_IMAGE_OPENEXR)
  list(APPEND INC_SYS
    ${OPENEXR_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENEXR_LIBRARIES}
  )
  add_definitions(-DWITH_OPENEXR)
  if(WIN32)
    if(EXISTS ${LIBDIR}/imath/bin/imath.dll)
      add_definitions(-DIMATH_DLL)
    endif()
  endif()
endif()

blender_add_lib(bf_imbuf_openexr "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
