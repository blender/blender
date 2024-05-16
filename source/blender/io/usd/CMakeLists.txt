# SPDX-FileCopyrightText: 2019 Blender Authors
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

# Precompiled Linux libs are made with GCC, and USD uses some extensions
# which lead to an incompatible ABI for Clang. Using those extensions with
# Clang as well works around the issue.
if(UNIX AND NOT APPLE)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(DEFINED LIBDIR)
      add_definitions(-DARCH_HAS_GNU_STL_EXTENSIONS)
    endif()
  endif()
endif()

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
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    # USD currently does not support the new preprocessor,
    # so we remove it here and disable sse2neon
    remove_cc_flag("/Zc:preprocessor")
    add_definitions(-DDISABLE_SSE2NEON)
  endif()
endif()

set(INC
  .
  ../common
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../editors/include
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../python/intern
  ../../windowmanager
  ../../../../intern/utfconv
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
  ${PYTHON_INCLUDE_DIR}

)

set(SRC
  intern/usd_armature_utils.cc
  intern/usd_asset_utils.cc
  intern/usd_attribute_utils.cc
  intern/usd_blend_shape_utils.cc
  intern/usd_capi_export.cc
  intern/usd_capi_import.cc
  intern/usd_hierarchy_iterator.cc
  intern/usd_hook.cc
  intern/usd_mesh_utils.cc
  intern/usd_writer_abstract.cc
  intern/usd_writer_armature.cc
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
  intern/usd_reader_instance.cc
  intern/usd_reader_light.cc
  intern/usd_reader_material.cc
  intern/usd_reader_mesh.cc
  intern/usd_reader_nurbs.cc
  intern/usd_reader_pointinstancer.cc
  intern/usd_reader_prim.cc
  intern/usd_reader_shape.cc
  intern/usd_reader_skeleton.cc
  intern/usd_reader_stage.cc
  intern/usd_reader_utils.cc
  intern/usd_reader_volume.cc
  intern/usd_reader_xform.cc
  intern/usd_skel_convert.cc
  intern/usd_skel_root_utils.cc

  usd.hh
  usd_private.hh

  intern/usd_armature_utils.hh
  intern/usd_asset_utils.hh
  intern/usd_attribute_utils.hh
  intern/usd_blend_shape_utils.hh
  intern/usd_exporter_context.hh
  intern/usd_hash_types.hh
  intern/usd_hierarchy_iterator.hh
  intern/usd_hook.hh
  intern/usd_mesh_utils.hh
  intern/usd_writer_abstract.hh
  intern/usd_writer_armature.hh
  intern/usd_writer_camera.hh
  intern/usd_writer_curves.hh
  intern/usd_writer_hair.hh
  intern/usd_writer_light.hh
  intern/usd_writer_material.hh
  intern/usd_writer_mesh.hh
  intern/usd_writer_metaball.hh
  intern/usd_writer_transform.hh
  intern/usd_writer_volume.hh

  intern/usd_reader_camera.hh
  intern/usd_reader_curve.hh
  intern/usd_reader_geom.hh
  intern/usd_reader_instance.hh
  intern/usd_reader_light.hh
  intern/usd_reader_material.hh
  intern/usd_reader_mesh.hh
  intern/usd_reader_nurbs.hh
  intern/usd_reader_pointinstancer.hh
  intern/usd_reader_prim.hh
  intern/usd_reader_shape.hh
  intern/usd_reader_skeleton.hh
  intern/usd_reader_stage.hh
  intern/usd_reader_utils.hh
  intern/usd_reader_volume.hh
  intern/usd_reader_xform.hh
  intern/usd_skel_convert.hh
  intern/usd_skel_root_utils.hh
)

if(WITH_HYDRA)
  list(APPEND SRC
    hydra/curves.cc
    hydra/hydra_scene_delegate.cc
    hydra/id.cc
    hydra/image.cc
    hydra/instancer.cc
    hydra/light.cc
    hydra/material.cc
    hydra/mesh.cc
    hydra/object.cc
    hydra/usd_scene_delegate.cc
    hydra/volume.cc
    hydra/volume_modifier.cc
    hydra/world.cc

    hydra/curves.hh
    hydra/hydra_scene_delegate.hh
    hydra/id.hh
    hydra/image.hh
    hydra/instancer.hh
    hydra/light.hh
    hydra/material.hh
    hydra/mesh.hh
    hydra/object.hh
    hydra/usd_scene_delegate.hh
    hydra/volume.hh
    hydra/volume_modifier.hh
    hydra/world.hh
  )
endif()

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::animrig
  bf_imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  PRIVATE bf::extern::fmtlib
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
  list(APPEND INC
    ../../../../intern/openvdb
  )
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

if(WITH_MATERIALX)
  add_definitions(-DWITH_MATERIALX)
  list(APPEND LIB MaterialXCore)
endif()

blender_add_lib(bf_io_usd "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_io_usd bf_rna)

if(COMMAND target_precompile_headers)
  target_precompile_headers(bf_io_usd PRIVATE intern/usd_precomp.hh)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/usd_curves_test.cc
    tests/usd_export_test.cc
    tests/usd_stage_creation_test.cc
    tests/usd_usdz_export_test.cc
    intern/usd_writer_material.hh
  )
  if(USD_IMAGING_HEADERS)
    list(APPEND TEST_SRC tests/usd_imaging_test.cc)
  endif()

  include_directories(intern)

  set(TEST_INC
  )
  set(TEST_LIB
    bf_blenloader_test_util
  )
  blender_add_test_suite_lib(io_usd "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()

# In CMAKE version 3.21 and up, we can instead use the `NO_CACHE` option for
# `find_file` so we don't need to clear it from the cache here.
unset(USD_IMAGING_HEADERS CACHE)
