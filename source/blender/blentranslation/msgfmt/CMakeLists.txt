# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2017 Blender Foundation. All rights reserved.

# -----------------------------------------------------------------------------
# Build msgfmt executable

blender_include_dirs(
  ../../../../intern/guardedalloc
  ../../blenlib
)

set(SRC
  msgfmt.c
)

add_cc_flags_custom_test(msgfmt)

if(WIN32)
  string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /nodefaultlib:MSVCRT.lib")
endif()

add_executable(msgfmt ${SRC})
setup_platform_linker_flags(msgfmt)

target_link_libraries(msgfmt bf_blenlib)
target_link_libraries(msgfmt bf_intern_guardedalloc)

if(WIN32)
  target_link_libraries(msgfmt bf_intern_utfconv)
endif()

target_link_libraries(msgfmt ${ZLIB_LIBRARIES})
target_link_libraries(msgfmt ${PLATFORM_LINKLIBS})
