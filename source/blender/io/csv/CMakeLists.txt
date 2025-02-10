# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  importer
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
  IO_csv.cc
  importer/csv_data.cc
  importer/csv_reader.cc

  IO_csv.hh
  importer/csv_data.hh
  importer/csv_reader.hh
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

blender_add_lib(bf_io_csv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
