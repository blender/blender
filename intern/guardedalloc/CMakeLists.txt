# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(HAVE_MALLOC_STATS_H)
  add_definitions(-DHAVE_MALLOC_STATS_H)
endif()

if(WITH_MEM_VALGRIND)
  add_definitions(-DWITH_MEM_VALGRIND)
endif()

set(INC
  PUBLIC .
)

set(INC_SYS

)

set(SRC
  ./intern/leak_detector.cc
  ./intern/mallocn.cc
  ./intern/mallocn_guarded_impl.cc
  ./intern/mallocn_lockfree_impl.cc
  ./intern/memory_usage.cc

  MEM_guardedalloc.h
  ./intern/mallocn_inline.h
  ./intern/mallocn_intern.h

  # only so the header is known by cmake
  ../atomic/atomic_ops.h
  ../atomic/intern/atomic_ops_ext.h
  ../atomic/intern/atomic_ops_msvc.h
  ../atomic/intern/atomic_ops_unix.h
  ../atomic/intern/atomic_ops_utils.h
)

set(LIB
  PRIVATE bf::intern::atomic
)

if(WIN32 AND NOT UNIX)
  list(APPEND INC_SYS
    ${PTHREADS_INC}
  )
  if(DEFINED PTHREADS_LIBRARIES)
    list(APPEND LIB
      ${PTHREADS_LIBRARIES}
    )
  endif()
endif()

# Jemalloc 5.0.0+ needs extra configuration.
if(WITH_MEM_JEMALLOC)
  if(NOT ("${JEMALLOC_VERSION}" VERSION_LESS "5.0.0"))
    add_definitions(-DWITH_JEMALLOC_CONF)
  endif()
endif()

blender_add_lib(bf_intern_guardedalloc "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::guardedalloc ALIAS bf_intern_guardedalloc)

# Override C++ alloc, optional.
if(WITH_CXX_GUARDEDALLOC)
  set(SRC
    cpp/mallocn.cpp
  )
  blender_add_lib(bf_intern_guardedalloc_cpp "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/guardedalloc_alignment_test.cc
    tests/guardedalloc_overflow_test.cc
    tests/guardedalloc_test_base.h
  )
  set(TEST_INC
    ../../source/blender/blenlib
  )
  set(TEST_LIB
    bf_intern_guardedalloc
    bf_blenlib
  )
  blender_add_test_suite_executable(guardedalloc "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
