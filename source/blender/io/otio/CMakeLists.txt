# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  exporter
  importer
  intern
  ../../editors/include
  ../../makesrna
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  IO_otio.cc
  exporter/otio_export.cc
  exporter/otio_export_resolve.cc
  importer/otio_import.cc
  importer/otio_import_kdenlive.cc
  importer/otio_import_resolve.cc
  intern/otio_metadata.cc
  intern/otio_resolve.cc

  IO_otio.hh
  exporter/otio_export.hh
  exporter/otio_export_resolve.hh
  importer/otio_import.hh
  importer/otio_import_kdenlive.hh
  importer/otio_import_resolve.hh
  intern/otio_metadata.hh
  intern/otio_resolve.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::imbuf
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
  PRIVATE bf::dependencies::optional::opentimelineio
)


blender_add_lib(bf_io_otio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_io_otio bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    tests/otio_round_trip_test.cc
  )

  set(TEST_INC
  )

  set(TEST_LIB
    bf_io_otio
    bf_blenloader_test_util
    PRIVATE bf::render
    PRIVATE bf::dependencies::optional::audaspace
    PRIVATE bf::dependencies::optional::ffmpeg
  )
  blender_add_test_suite_lib(io_otio "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
