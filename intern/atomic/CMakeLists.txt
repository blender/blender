# SPDX-FileCopyrightText: 2020 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
)
set(LIB
)

add_library(bf_intern_atomic INTERFACE)

target_include_directories(bf_intern_atomic INTERFACE .)
add_library(bf::intern::atomic ALIAS bf_intern_atomic)

# CMake 3.19+ allows one to populate the interface library with
# source files to show in the IDE, for people on older CMake versions
# these headers will be visible in the bf_intern_guardedalloc project
# where they historically have been.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19")
  set(SRC
    atomic_ops.h
    intern/atomic_ops_ext.h
    intern/atomic_ops_msvc.h
    intern/atomic_ops_unix.h
    intern/atomic_ops_utils.h
  )
  target_sources(bf_intern_atomic PRIVATE ${SRC})
  blender_source_group(bf_intern_atomic ${SRC})
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/atomic_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    PRIVATE bf_intern_atomic
  )
  include(GTestTesting)
  blender_add_test_executable(atomic "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
