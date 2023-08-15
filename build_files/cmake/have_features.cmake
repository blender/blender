# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This file is used to test the system for headers & symbols.
# Variables should use the `HAVE_` prefix.
# Defines should use the same name as the CMAKE variable.

include(CheckSymbolExists)

# Used for: `intern/guardedalloc/intern/mallocn_intern.h`.
# Function `malloc_stats` is only available on GLIBC,
# so check that before defining `HAVE_MALLOC_STATS`.
check_symbol_exists(malloc_stats "malloc.h" HAVE_MALLOC_STATS_H)

# Used for: `source/creator/creator_signals.c`.
# The function `feenableexcept` is not present non-GLIBC systems,
# hence we need to check if it's available in the `fenv.h` file.
set(HAVE_FEENABLEEXCEPT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  check_symbol_exists(feenableexcept "fenv.h" HAVE_FEENABLEEXCEPT)
endif()

# Used for: `source/blender/blenlib/intern/system.c`.
# `execinfo` is not available on non-GLIBC systems (at least not on MUSL-LIBC),
# so check the presence of the header before including it and using the it for back-trace.
set(HAVE_EXECINFO_H OFF)
if(NOT MSVC)
  include(CheckIncludeFiles)
  check_include_files("execinfo.h" HAVE_EXECINFO_H)
  if(HAVE_EXECINFO_H)
    add_definitions(-DHAVE_EXECINFO_H)
  endif()
endif()
