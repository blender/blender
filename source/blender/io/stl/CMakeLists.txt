# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ./importer
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
    IO_stl.cc
    importer/stl_import.cc
    importer/stl_import_ascii_reader.cc
    importer/stl_import_binary_reader.cc
    importer/stl_import_mesh.cc

    IO_stl.h
    importer/stl_import.hh
    importer/stl_import_ascii_reader.hh
    importer/stl_import_binary_reader.hh
    importer/stl_import_mesh.hh
)

set(LIB
  bf_blenkernel
  bf_io_common
)

blender_add_lib(bf_stl "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
