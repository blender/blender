# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  exporter
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
  intern/otio_metadata.cc
  intern/otio_resolve.cc

  IO_otio.hh
  exporter/otio_export.hh
  exporter/otio_export_resolve.hh
  intern/otio_metadata.hh
  intern/otio_resolve.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
  PRIVATE bf::dependencies::optional::opentimelineio
)


blender_add_lib(bf_io_otio "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_dependencies(bf_io_otio bf_rna)

