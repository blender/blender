# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../common
  ../../makesrna
)

set(INC_SYS
)

set(SRC
  IO_spz.cc
  importer/spz_import.cc

  IO_spz.hh
  importer/spz_import.hh

  intern/spz_header.cc
  intern/spz_header.hh
  intern/spz_read.cc
  intern/spz_read.hh
  intern/spz_read_common.cc
  intern/spz_read_common.hh
  intern/spz_read_gzip.cc
  intern/spz_read_ngsp.cc
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::blenkernel
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::depsgraph
  PRIVATE bf_io_common
  PRIVATE bf::intern::guardedalloc  # Indirect dependency of the `bf::blenkernel`.
  PRIVATE bf::intern::clog
  PRIVATE bf::dependencies::zlib
  PRIVATE bf::dependencies::zstd
)

blender_add_lib(bf_io_spz "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/spz_header_test.cc
  )
  set(TEST_LIB
    bf_io_spz
  )
  blender_add_test_suite_lib(io_spz "${TEST_SRC}" "${INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
