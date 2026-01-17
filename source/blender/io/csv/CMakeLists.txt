# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  importer
  ../common
  ../../blenkernel
)

set(INC_SYS
)

set(SRC
  importer/csv_reader.cc

  IO_csv.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::fast_float
)

blender_add_lib(bf_io_csv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
