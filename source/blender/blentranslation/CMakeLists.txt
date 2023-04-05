# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2008 Blender Foundation

set(INC
  .
  ../blenkernel
  ../blenlib
  ../imbuf
  ../makesdna
  ../makesrna
  ../../../intern/guardedalloc
  ../../../intern/locale
)

set(INC_SYS
)

set(SRC
  intern/blt_lang.c
  intern/blt_translation.c

  BLT_lang.h
  BLT_translation.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
  list(APPEND LIB
    bf_intern_locale
  )
endif()

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

if(WIN32 OR APPLE)
  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)
  endif()
endif()

blender_add_lib(bf_blentranslation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_INTERNATIONAL)
  add_subdirectory(msgfmt)
endif()
