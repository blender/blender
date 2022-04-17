# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2020 Blender Foundation. All rights reserved.

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../depsgraph
  ../../makesdna
  ../../../../intern/guardedalloc
  ../../../../extern/fast_float
)

set(INC_SYS
)

set(SRC
  intern/abstract_hierarchy_iterator.cc
  intern/dupli_parent_finder.cc
  intern/dupli_persistent_id.cc
  intern/object_identifier.cc
  intern/string_utils.cc

  IO_abstract_hierarchy_iterator.h
  IO_dupli_persistent_id.hh
  IO_string_utils.hh
  IO_types.h
  intern/dupli_parent_finder.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

blender_add_lib(bf_io_common "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

target_link_libraries(bf_io_common INTERFACE)

if(WITH_GTESTS)
  set(TEST_SRC
    intern/abstract_hierarchy_iterator_test.cc
    intern/hierarchy_context_order_test.cc
    intern/object_identifier_test.cc
    intern/string_utils_test.cc
  )
  set(TEST_INC
    ../../blenloader
  )
  set(TEST_LIB
    bf_blenloader_tests
    bf_io_common
  )
  include(GTestTesting)
  blender_add_test_lib(bf_io_common_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
