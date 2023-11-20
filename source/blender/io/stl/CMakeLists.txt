# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  importer
  exporter
  ../common
  ../../blenkernel
  ../../bmesh
  ../../bmesh/intern
  ../../depsgraph
  ../../editors/include
  ../../makesrna
  ../../nodes
  ../../windowmanager
)

set(INC_SYS
  ../../../../extern/fast_float
  ../../../../extern/fmtlib/include
)

set(SRC
  IO_stl.cc
  importer/stl_import.cc
  importer/stl_import_ascii_reader.cc
  importer/stl_import_binary_reader.cc
  importer/stl_import_mesh.cc
  exporter/stl_export.cc
  exporter/stl_export_writer.cc

  IO_stl.hh
  importer/stl_import.hh
  importer/stl_import_ascii_reader.hh
  importer/stl_import_binary_reader.hh
  importer/stl_import_mesh.hh
  exporter/stl_export_writer.hh
  exporter/stl_export.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  extern_fmtlib
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

    bf_blenloader_tests
    bf_io_stl
  )

  include(GTestTesting)
  blender_add_test_lib(bf_io_stl_tests "${TEST_SRC}" "${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
  add_dependencies(bf_io_stl_tests bf_io_stl)
endif()
