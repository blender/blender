# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2016 Blender Foundation. All rights reserved.

# Too noisy for code we don't maintain.
if(CMAKE_COMPILER_IS_GNUCC)
  if(NOT "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "8.0")
    add_cxx_flag("-Wno-cast-function-type")
  endif()
endif()

set(INC
  src
  src/gflags
)

set(INC_SYS
)

set(SRC
  src/gflags.cc
  src/gflags_completions.cc
  src/gflags_reporting.cc

  src/gflags/config.h
  src/gflags/gflags_completions.h
  src/gflags/gflags_declare.h
  src/gflags/gflags_gflags.h
  src/gflags/gflags.h
  src/mutex.h
  src/util.h
)

set(LIB
)

if(WIN32)
  list(APPEND SRC
    src/windows_port.cc
    src/windows_port.h
  )
endif()

add_definitions(${GFLAGS_DEFINES})

blender_add_lib(extern_gflags "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
