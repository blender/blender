# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../io/alembic
  ../../io/collada
  ../../io/common
  ../../io/gpencil
  ../../io/ply
  ../../io/stl
  ../../io/usd
  ../../io/wavefront_obj
  ../../makesrna
  ../../windowmanager
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS

)

set(SRC
  io_alembic.cc
  io_cache.cc
  io_collada.cc
  io_drop_import_file.cc
  io_gpencil_export.cc
  io_gpencil_import.cc
  io_gpencil_utils.cc
  io_obj.cc
  io_ops.cc
  io_ply_ops.cc
  io_stl_ops.cc
  io_usd.cc
  io_utils.cc

  io_alembic.hh
  io_cache.hh
  io_collada.hh
  io_drop_import_file.hh
  io_gpencil.hh
  io_obj.hh
  io_ops.hh
  io_ply_ops.hh
  io_stl_ops.hh
  io_usd.hh
  io_utils.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
)

if(WITH_OPENCOLLADA)
  list(APPEND LIB
    bf_io_collada
  )
  add_definitions(-DWITH_COLLADA)
endif()

if(WITH_IO_WAVEFRONT_OBJ)
  list(APPEND LIB
    bf_io_wavefront_obj
  )
  add_definitions(-DWITH_IO_WAVEFRONT_OBJ)
endif()

if(WITH_IO_PLY)
  list(APPEND LIB
    bf_io_ply
  )
  add_definitions(-DWITH_IO_PLY)
endif()

if(WITH_IO_STL)
  list(APPEND LIB
    bf_io_stl
  )
  add_definitions(-DWITH_IO_STL)
endif()

if(WITH_IO_GPENCIL)
  list(APPEND LIB
    bf_gpencil
  )
  add_definitions(-DWITH_IO_GPENCIL)
endif()

if(WITH_ALEMBIC)
  list(APPEND LIB
    bf_io_alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_USD)
  list(APPEND LIB
    bf_io_usd
  )
  add_definitions(-DWITH_USD)
endif()

if(WITH_PUGIXML)
  add_definitions(-DWITH_PUGIXML)
endif()

if(WITH_HARU)
  add_definitions(-DWITH_HARU)
endif()

blender_add_lib(bf_editor_io "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
