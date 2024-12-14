# SPDX-FileCopyrightText: 2012 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
)

set(SRC
  blender_locale.cpp
  messages.cpp

  blender_locale.h
  messages.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::intern::guardedalloc
)

if(APPLE)
  # Cocoa code to read the locale on OSX
  list(APPEND SRC
    messages_apple.mm
  )
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_GHOST_SDL)
  add_definitions(-DWITH_GHOST_SDL)
endif()

blender_add_lib(bf_intern_locale "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
