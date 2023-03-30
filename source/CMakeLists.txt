# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation

if(WITH_LEGACY_OPENGL)
  add_definitions(-DWITH_LEGACY_OPENGL)
endif()

if(WITH_CLANG_TIDY AND NOT MSVC)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Currently Clang-Tidy might fail with GCC toolchain, switch to Clang toolchain if that happens")
    if(COMMAND target_precompile_headers)
      message(STATUS "Clang-Tidy and GCC precompiled headers are incompatible, disabling precompiled headers")
      set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
    endif()
  endif()

  find_package(ClangTidy REQUIRED)
  set(CMAKE_C_CLANG_TIDY
    ${CLANG_TIDY_EXECUTABLE};--extra-arg=-Wno-error=unknown-warning-option)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE};--extra-arg=-Wno-error=unknown-warning-option)
endif()

add_subdirectory(blender)
