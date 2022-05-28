# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2019 Blender Foundation. All rights reserved.

# This suppresses the warning "This file includes at least one deprecated or antiquated
# header which may be removed without further notice at a future date", which is caused
# by the USD library including <ext/hash_set> on Linux. This has been reported at:
# https://github.com/PixarAnimationStudios/USD/issues/1057.
if(UNIX AND NOT APPLE)
  add_definitions(-D_GLIBCXX_PERMIT_BACKWARD_HASH)
endif()
if(WIN32)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()
add_definitions(-DPXR_STATIC)

# USD headers use deprecated TBB headers, silence warning.
add_definitions(-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1)

# Check if USD has the imaging headers available, if they are
# add a USD_HAS_IMAGING define so code can dynamically detect this.
# Cleanup of this variable is done at the end of the file since
# test code further down uses it to add imaging tests.
FIND_FILE(USD_IMAGING_HEADERS
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

set(INC
  .
  ../common
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
)

set(SRC
  intern/usd_capi_export.cc
  intern/usd_capi_import.cc
  intern/usd_common.cc
  intern/usd_hierarchy_iterator.cc
  intern/usd_writer_abstract.cc
  intern/usd_writer_camera.cc
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
  intern/usd_reader_stage.cc
  intern/usd_reader_volume.cc
  intern/usd_reader_xform.cc

  usd.h

  intern/usd_common.h
  intern/usd_exporter_context.h
  intern/usd_hierarchy_iterator.h
  intern/usd_writer_abstract.h
  intern/usd_writer_camera.h
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
  intern/usd_reader_stage.h
  intern/usd_reader_volume.h
  intern/usd_reader_xform.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_imbuf
  bf_io_common
)

list(APPEND LIB
  ${BOOST_LIBRARIES}
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

if(WIN32)
  set_property(TARGET bf_usd APPEND_STRING PROPERTY INTERFACE_LINK_OPTIONS "$<$<CONFIG:Debug>:/WHOLEARCHIVE:${USD_DEBUG_LIB}>")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY INTERFACE_LINK_OPTIONS "$<$<CONFIG:Release>:/WHOLEARCHIVE:${USD_RELEASE_LIB}>")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY INTERFACE_LINK_OPTIONS "$<$<CONFIG:RelWithDebInfo>:/WHOLEARCHIVE:${USD_RELEASE_LIB}>")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY INTERFACE_LINK_OPTIONS "$<$<CONFIG:MinSizeRel>:/WHOLEARCHIVE:${USD_RELEASE_LIB}>")
endif()

# Source: https://github.com/PixarAnimationStudios/USD/blob/master/BUILDING.md#linking-whole-archives
if(WIN32)
  target_link_libraries(bf_usd INTERFACE ${USD_LIBRARIES})
elseif(APPLE)
  target_link_libraries(bf_usd INTERFACE -Wl,-force_load ${USD_LIBRARIES})
elseif(UNIX)
  target_link_libraries(bf_usd INTERFACE "-Wl,--whole-archive ${USD_LIBRARIES} -Wl,--no-whole-archive ${TBB_LIBRARIES}")
else()
  message(FATAL_ERROR "Unknown how to link USD with your compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

target_link_libraries(bf_usd INTERFACE ${TBB_LIBRARIES})

if(WITH_GTESTS)
  set(TEST_SRC
    tests/usd_stage_creation_test.cc
    tests/usd_tests_common.cc
    tests/usd_tests_common.h
  )
  if(USD_IMAGING_HEADERS)
    LIST(APPEND TEST_SRC tests/usd_imaging_test.cc)
  endif()

  set(TEST_INC
  )
  set(TEST_LIB
  )
  include(GTestTesting)
  blender_add_test_lib(bf_io_usd_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()

# In cmake version 3.21 and up, we can instead use the NO_CACHE option for
# find_file so we don't need to clear it from the cache here.
unset(USD_IMAGING_HEADERS CACHE)
