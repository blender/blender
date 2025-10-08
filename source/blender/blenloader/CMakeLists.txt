# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../editors/include
  ../makesrna

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZSTD_INCLUDE_DIRS}
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default_theme.c
  intern/blend_validate.cc
  intern/readblenentry.cc
  intern/readfile.cc
  intern/readfile_tempload.cc
  intern/undofile.cc
  intern/versioning_250.cc
  intern/versioning_260.cc
  intern/versioning_270.cc
  intern/versioning_280.cc
  intern/versioning_290.cc
  intern/versioning_300.cc
  intern/versioning_400.cc
  intern/versioning_410.cc
  intern/versioning_420.cc
  intern/versioning_430.cc
  intern/versioning_440.cc
  intern/versioning_450.cc
  intern/versioning_500.cc
  intern/versioning_510.cc
  intern/versioning_common.cc
  intern/versioning_defaults.cc
  intern/versioning_dna.cc
  intern/versioning_legacy.cc
  intern/versioning_userdef.cc
  intern/writefile.cc

  BLO_blend_validate.hh
  BLO_read_write.hh
  BLO_readfile.hh
  BLO_undofile.hh
  BLO_userdef_default.h
  BLO_writefile.hh
  intern/readfile.hh
  intern/versioning_common.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PUBLIC bf::blenloader_core
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::memutil
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
  PRIVATE bf::extern::xxhash
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_ALEMBIC)
  list(APPEND INC
    ../io/alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

blender_add_lib(bf_blenloader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::blenloader ALIAS bf_blenloader)

# RNA_prototypes.hh
add_dependencies(bf_blenloader bf_rna)

if(WITH_GTESTS)
  # Utility functions for test also used by other tests.
  set(TEST_UTIL_SRC
    tests/blendfile_loading_base_test.cc
    tests/blendfile_loading_base_test.h
  )
  set(TEST_UTIL_INC
    ${INC}
    ../../../tests/gtests
    ../../../intern/ghost
  )
  set(TEST_UTIL_INC_SYS
    ${INC_SYS}
    ${GFLAGS_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/extern/gtest/include
  )
  set(TEST_UTIL_LIB
    ${LIB}
    PRIVATE bf::blenfont
    bf_blenloader
  )
  blender_add_lib(bf_blenloader_test_util "${TEST_UTIL_SRC}" "${TEST_UTIL_INC}" "${TEST_UTIL_INC_SYS}" "${TEST_UTIL_LIB}")

  # Actual `blenloader` tests.
  set(TEST_SRC
    tests/blendfile_load_test.cc
  )
  set(TEST_LIB
    ${LIB}
    bf_blenloader
    bf_blenloader_test_util
  )
  blender_add_test_suite_lib(blenloader "${TEST_SRC}" "${INC}" "${INC_SYS}" "${TEST_LIB}")
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_EXPERIMENTAL_FEATURES)
endif()
