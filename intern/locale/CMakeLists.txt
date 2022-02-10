# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2012 Blender Foundation. All rights reserved.

set(INC
  .
)

set(INC_SYS
)

set(SRC
  boost_locale_wrapper.cpp

  boost_locale_wrapper.h
)

set(LIB
)

if(APPLE)
  # Cocoa code to read the locale on OSX
  list(APPEND SRC
    osx_user_locale.mm
  )
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_GHOST_SDL)
  add_definitions(-DWITH_GHOST_SDL)
endif()

if(WITH_INTERNATIONAL)
  list(APPEND INC_SYS
    ${BOOST_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${BOOST_LIBRARIES}
  )
  add_definitions(${BOOST_DEFINITIONS})
endif()

blender_add_lib(bf_intern_locale "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
