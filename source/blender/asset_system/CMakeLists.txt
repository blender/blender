# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  intern
  intern/library_types
)

set(INC_SYS
)

set(SRC
  intern/asset_catalog.cc
  intern/asset_catalog_collection.cc
  intern/asset_catalog_definition_file.cc
  intern/asset_catalog_path.cc
  intern/asset_catalog_tree.cc
  intern/asset_library.cc
  intern/asset_library_service.cc
  intern/asset_representation.cc
  intern/library_types/all_library.cc
  intern/library_types/essentials_library.cc
  intern/library_types/on_disk_library.cc
  intern/library_types/preferences_on_disk_library.cc
  intern/library_types/runtime_library.cc
  intern/utils.cc

  AS_asset_catalog.hh
  AS_asset_catalog_path.hh
  AS_asset_catalog_tree.hh
  AS_asset_library.hh
  AS_asset_representation.hh
  AS_essentials_library.hh
  intern/asset_catalog_collection.hh
  intern/asset_catalog_definition_file.hh
  intern/asset_library_service.hh
  intern/library_types/all_library.hh
  intern/library_types/essentials_library.hh
  intern/library_types/on_disk_library.hh
  intern/library_types/preferences_on_disk_library.hh
  intern/library_types/runtime_library.hh
  intern/utils.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
)


blender_add_lib(bf_asset_system "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::asset_system ALIAS bf_asset_system)

if(WITH_GTESTS)
  set(TEST_INC
    ../editors/asset
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
    PRIVATE bf_editor_asset
  )
  blender_add_test_suite_lib(asset_system
    "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}" "${TEST_COMMON_SRC}"
  )
endif()
