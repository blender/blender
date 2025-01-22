# SPDX-FileCopyrightText: 2008 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../makesrna
)

set(INC_SYS
)

set(SRC
  intern/blt_lang.cc
  intern/blt_translation.cc

  BLT_lang.hh
  BLT_translation.hh
)

if(WITH_INTERNATIONAL)
  list(APPEND SRC
    intern/messages.cc
    intern/messages.hh
  )

  if(APPLE)
    list(APPEND SRC
      intern/messages_apple.mm
    )
  endif()
endif()

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

blender_add_lib(bf_blentranslation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blentranslation ALIAS bf_blentranslation)

if(WITH_INTERNATIONAL)
  add_subdirectory(msgfmt)
endif()
