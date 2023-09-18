# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenfont
  ../blenkernel
  ../blentranslation
  ../depsgraph
  ../draw
  ../editors/include
  ../gpu
  ../imbuf
  ../makesrna
  ../nodes
  ../render
  ../sequencer
  ../windowmanager
  ../../../intern/clog
  ../bmesh

  # RNA_prototypes.h
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
  intern/versioning_common.cc
  intern/versioning_defaults.cc
  intern/versioning_dna.cc
  intern/versioning_legacy.cc
  intern/versioning_userdef.cc
  intern/writefile.cc

  BLO_blend_defs.hh
  BLO_blend_validate.hh
  BLO_read_write.hh
  BLO_readfile.h
  BLO_undofile.hh
  BLO_writefile.hh
  intern/readfile.hh
  intern/versioning_common.hh
)

set(LIB
  PRIVATE bf::animrig
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
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

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

blender_add_lib(bf_blenloader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_blenloader bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/blendfile_load_test.cc
    tests/blendfile_loading_base_test.cc

    tests/blendfile_loading_base_test.h
  )
  set(TEST_INC
    ../../../intern/ghost
  )
  set(TEST_LIB
    bf_blenloader
  )
  include(GTestTesting)
  blender_add_test_lib(bf_blenloader_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
