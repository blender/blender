# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_LEGACY_OPENGL)
  add_definitions(-DWITH_LEGACY_OPENGL)
endif()

if(WITH_CLANG_TIDY AND NOT MSVC)
  find_package(ClangTidy REQUIRED)
  set(CLANG_TIDY_EXTRA_ARGS --extra-arg=-Wno-error=unknown-warning-option)

  set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE};${CLANG_TIDY_EXTRA_ARGS})
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE};${CLANG_TIDY_EXTRA_ARGS})
  set(CMAKE_OBJC_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE};${CLANG_TIDY_EXTRA_ARGS})
  set(CMAKE_OBJCXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE};${CLANG_TIDY_EXTRA_ARGS})
endif()

add_subdirectory(blender)
