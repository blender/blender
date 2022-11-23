# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  exporter
  importer
  ../common
  ../../blenkernel
  ../../blenlib
  ../../bmesh
  ../../bmesh/intern
  ../../depsgraph
  ../../editors/include
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../windowmanager
  ../../../../extern/fast_float
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
    IO_ply.cc
    exporter/ply_export.cc
    importer/ply_import.cc



    IO_ply.h
    exporter/ply_export.hh
    importer/ply_import.hh

    intern/ply_data.hh
)

set(LIB
  bf_blenkernel
  bf_io_common
)

blender_add_lib(bf_ply "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
