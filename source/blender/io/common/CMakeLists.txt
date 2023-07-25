# SPDX-FileCopyrightText: 2020 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  intern/abstract_hierarchy_iterator.cc
  intern/dupli_parent_finder.cc
  intern/dupli_persistent_id.cc
  intern/object_identifier.cc
  intern/orientation.c
  intern/path_util.cc

  IO_abstract_hierarchy_iterator.h
  IO_dupli_persistent_id.hh
  IO_orientation.h
  IO_path_util.hh
  IO_path_util_types.h
  IO_types.h
  intern/dupli_parent_finder.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_io_common "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

target_link_libraries(bf_io_common INTERFACE)

if(WITH_GTESTS)
  set(TEST_SRC
    intern/abstract_hierarchy_iterator_test.cc
    intern/hierarchy_context_order_test.cc
    intern/object_identifier_test.cc
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
