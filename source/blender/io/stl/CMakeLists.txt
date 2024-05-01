# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  exporter
  importer
  intern
  ../common
  ../../blenkernel
  ../../bmesh
  ../../editors/include
  ../../makesrna
  ../../windowmanager
)

set(INC_SYS
  ../../../../extern/fast_float
)

set(SRC
  IO_stl.cc
  importer/stl_import.cc
  importer/stl_import_ascii_reader.cc
  importer/stl_import_binary_reader.cc
  importer/stl_import_mesh.cc
  exporter/stl_export.cc
  exporter/stl_export_writer.cc

  intern/stl_data.hh

  IO_stl.hh
  importer/stl_import.hh
  importer/stl_import_ascii_reader.hh
  importer/stl_import_binary_reader.hh
  importer/stl_import_mesh.hh
  exporter/stl_export.hh
  exporter/stl_export_writer.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  PRIVATE bf::extern::fmtlib
)

blender_add_lib(bf_io_stl "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/stl_exporter_tests.cc
    tests/stl_importer_tests.cc
  )

  set(TEST_INC
    ${INC}

    ../../blenloader
    ../../../../tests/gtests
  )

  set(TEST_LIB
    ${LIB}

    bf_blenloader_test_util
    bf_io_stl
  )

  blender_add_test_suite_lib(io_stl "${TEST_SRC}" "${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()
