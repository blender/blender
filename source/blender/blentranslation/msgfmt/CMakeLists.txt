# SPDX-FileCopyrightText: 2017 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# Build msgfmt executable

set(INC
)

set(SRC
  msgfmt.cc
)

set(LIB
  PRIVATE PRIVATE bf::blenlib
  PRIVATE bf::intern::guardedalloc
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
