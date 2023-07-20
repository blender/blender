# SPDX-FileCopyrightText: 2019 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This suppresses the warning "This file includes at least one deprecated or antiquated
# header which may be removed without further notice at a future date", which is caused
# by the USD library including <ext/hash_set> on Linux. This has been reported at:
# https://github.com/PixarAnimationStudios/USD/issues/1057.
if(UNIX AND NOT APPLE)
  add_definitions(-D_GLIBCXX_PERMIT_BACKWARD_HASH)
endif()
if(WIN32)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -DBOOST_DEBUG_PYTHON)
endif()
add_definitions(-DBOOST_ALL_NO_LIB)

# USD headers use deprecated TBB headers, silence warning.
add_definitions(-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1)

# Check if USD has the imaging headers available, if they are
# add a USD_HAS_IMAGING define so code can dynamically detect this.
# Cleanup of this variable is done at the end of the file since
# test code further down uses it to add imaging tests.
find_file(
  USD_IMAGING_HEADERS
  NAMES
    capsuleAdapter.h
  PATHS
    ${USD_INCLUDE_DIRS}
  PATH_SUFFIXES
    pxr/usdImaging/usdImaging/
  NO_DEFAULT_PATH
)

if(USD_IMAGING_HEADERS)
  add_definitions(-DUSD_HAS_IMAGING)
endif()

if(WIN32)
  # Some USD library headers trigger the "unreferenced formal parameter"
  # warning alert.
  # Silence them by restore warn C4100 back to w4
  remove_cc_flag("/w34100")
endif()

set(INC
  .
  ../common
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../editors/include
  ../../imbuf
  ../../makesrna
  ../../../../intern/utfconv
)

set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
  ${PYTHON_INCLUDE_DIR}
)

set(SRC
  intern/usd_asset_utils.cc
  intern/usd_capi_export.cc
  intern/usd_capi_import.cc
  intern/usd_hierarchy_iterator.cc
  intern/usd_writer_abstract.cc
  intern/usd_writer_camera.cc
  intern/usd_writer_curves.cc
  intern/usd_writer_hair.cc
  intern/usd_writer_light.cc
  intern/usd_writer_material.cc
  intern/usd_writer_mesh.cc
  intern/usd_writer_metaball.cc
  intern/usd_writer_transform.cc
  intern/usd_writer_volume.cc

  intern/usd_reader_camera.cc
  intern/usd_reader_curve.cc
  intern/usd_reader_geom.cc
  intern/usd_reader_light.cc
  intern/usd_reader_material.cc
  intern/usd_reader_mesh.cc
  intern/usd_reader_nurbs.cc
  intern/usd_reader_prim.cc
  intern/usd_reader_shape.cc
  intern/usd_reader_stage.cc
  intern/usd_reader_volume.cc
  intern/usd_reader_xform.cc

  usd.h

  intern/usd_asset_utils.h
  intern/usd_exporter_context.h
  intern/usd_hierarchy_iterator.h
  intern/usd_writer_abstract.h
  intern/usd_writer_camera.h
  intern/usd_writer_curves.h
  intern/usd_writer_hair.h
  intern/usd_writer_light.h
  intern/usd_writer_material.h
  intern/usd_writer_mesh.h
  intern/usd_writer_metaball.h
  intern/usd_writer_transform.h
  intern/usd_writer_volume.h

  intern/usd_reader_camera.h
  intern/usd_reader_curve.h
  intern/usd_reader_geom.h
  intern/usd_reader_light.h
  intern/usd_reader_material.h
  intern/usd_reader_mesh.h
  intern/usd_reader_nurbs.h
  intern/usd_reader_prim.h
  intern/usd_reader_shape.h
  intern/usd_reader_stage.h
  intern/usd_reader_volume.h
  intern/usd_reader_xform.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_imbuf
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  PRIVATE bf::windowmanager
)

list(APPEND LIB
  ${BOOST_LIBRARIES}
  ${BOOST_PYTHON_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${USD_LIBRARIES}
  ${TBB_LIBRARIES}
)

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

blender_add_lib(bf_usd "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(COMMAND target_precompile_headers)
  target_precompile_headers(bf_usd PRIVATE intern/usd_precomp.h)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/usd_curves_test.cc
    tests/usd_export_test.cc
    tests/usd_stage_creation_test.cc
    tests/usd_usdz_export_test.cc
    intern/usd_writer_material.h
  )
  if(USD_IMAGING_HEADERS)
    list(APPEND TEST_SRC tests/usd_imaging_test.cc)
  endif()

  include_directories(intern)

  set(TEST_INC
  )
  set(TEST_LIB
  )
  include(GTestTesting)
  blender_add_test_lib(bf_io_usd_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()

# In CMAKE version 3.21 and up, we can instead use the `NO_CACHE` option for
# `find_file` so we don't need to clear it from the cache here.
unset(USD_IMAGING_HEADERS CACHE)
