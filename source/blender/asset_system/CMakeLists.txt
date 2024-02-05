# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenkernel
)

set(INC_SYS
)

set(SRC
  intern/asset_catalog.cc
  intern/asset_catalog_path.cc
  intern/asset_catalog_tree.cc
  intern/asset_essentials_library.cc
  intern/asset_identifier.cc
  intern/asset_library.cc
  intern/asset_library_all.cc
  intern/asset_library_on_disk.cc
  intern/asset_library_runtime.cc
  intern/asset_library_service.cc
  intern/asset_representation.cc
  intern/asset_storage.cc
  intern/utils.cc

  AS_asset_catalog.hh
  AS_asset_catalog_path.hh
  AS_asset_catalog_tree.hh
  AS_asset_identifier.hh
  AS_asset_library.hh
  AS_asset_representation.hh
  AS_essentials_library.hh
  intern/asset_library_all.hh
  intern/asset_library_on_disk.hh
  intern/asset_library_runtime.hh
  intern/asset_library_service.hh
  intern/asset_storage.hh
  intern/utils.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_asset_system "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    tests/asset_catalog_path_test.cc
    tests/asset_catalog_test.cc
    tests/asset_catalog_tree_test.cc
    tests/asset_library_service_test.cc
    tests/asset_library_test.cc
    tests/asset_representation_test.cc
  )
  set(TEST_COMMON_SRC
    tests/asset_library_test_common.hh
  )
  set(TEST_LIB
    bf_asset_system
  )
  blender_add_test_suite_lib(asset_system
    "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}" "${TEST_COMMON_SRC}"
  )
endif()
