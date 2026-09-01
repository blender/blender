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

  intern/spz_read.cc
  intern/spz_read_gzip.cc
  intern/spz_read_ngsp.cc
  intern/spz_read.hh
  intern/spz_read_common.cc
  intern/spz_read_common.hh
  intern/spz_types.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::blenkernel
  PRIVATE bf::dna
  PRIVATE bf::depsgraph
  PRIVATE bf_io_common
  PRIVATE bf::intern::guardedalloc  # Indirect dependnecy of the bf::blenkernel.
  PRIVATE bf::intern::clog
  PRIVATE bf::dependencies::zlib
  PRIVATE bf::dependencies::zstd
)

blender_add_lib(bf_io_spz "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
