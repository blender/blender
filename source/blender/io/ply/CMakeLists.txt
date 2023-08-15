# SPDX-FileCopyrightText: 2023 Blender Foundation
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
  ../../depsgraph
  ../../geometry
  ../../makesrna
  ../../windowmanager
)

set(INC_SYS
  ../../../../extern/fast_float
  ../../../../extern/fmtlib/include
)

set(SRC
  exporter/ply_export.cc
  exporter/ply_export_data.cc
  exporter/ply_export_header.cc
  exporter/ply_export_load_plydata.cc
  exporter/ply_file_buffer.cc
  exporter/ply_file_buffer_ascii.cc
  exporter/ply_file_buffer_binary.cc
  importer/ply_import.cc
  importer/ply_import_buffer.cc
  importer/ply_import_data.cc
  importer/ply_import_mesh.cc
  IO_ply.cc

  exporter/ply_export.hh
  exporter/ply_export_data.hh
  exporter/ply_export_header.hh
  exporter/ply_export_load_plydata.hh
  exporter/ply_file_buffer.hh
  exporter/ply_file_buffer_ascii.hh
  exporter/ply_file_buffer_binary.hh
  importer/ply_import.hh
  importer/ply_import_buffer.hh
  importer/ply_import_data.hh
  importer/ply_import_mesh.hh
  IO_ply.h

  intern/ply_data.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  extern_fmtlib
)

blender_add_lib(bf_ply "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/io_ply_exporter_test.cc
    tests/io_ply_importer_test.cc
  )
  set(TEST_INC
    ../../blenloader
    ../../../../tests/gtests
  )
  set(TEST_LIB
    bf_ply
  )
  include(GTestTesting)
  blender_add_test_lib(bf_io_ply_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
