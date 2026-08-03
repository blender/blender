# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  INTERFACE .
)

set(INC_SYS
)
set(LIB
)

set(SRC
  atomic_ops.h
  intern/atomic_ops_ext.h
  intern/atomic_ops_msvc.h
  intern/atomic_ops_unix.h
  intern/atomic_ops_utils.h
)

blender_add_lib(bf_intern_atomic "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::atomic ALIAS bf_intern_atomic)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/atomic_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    PRIVATE bf_intern_atomic
  )
  blender_add_test_executable(atomic "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
