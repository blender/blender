# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2017 Blender Foundation

# -----------------------------------------------------------------------------
# Build msgfmt executable

set(INC
  ../../../../intern/guardedalloc
  ../../blenlib
)

set(SRC
  msgfmt.c
)

set(LIB
  bf_blenlib
  bf_intern_guardedalloc
  ${ZLIB_LIBRARIES}
  ${PLATFORM_LINKLIBS})

add_cc_flags_custom_test(msgfmt)

if(WIN32)
  string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /nodefaultlib:MSVCRT.lib")
  list(APPEND LIB bf_intern_utfconv)
endif()

add_executable(msgfmt ${SRC})
setup_platform_linker_flags(msgfmt)
blender_target_include_dirs(msgfmt ${INC})
target_link_libraries(msgfmt ${LIB})
