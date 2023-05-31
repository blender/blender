# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../io/alembic
  ../../io/collada
  ../../io/common
  ../../io/gpencil
  ../../io/ply
  ../../io/stl
  ../../io/usd
  ../../io/wavefront_obj
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  io_alembic.c
  io_cache.c
  io_collada.c
  io_gpencil_export.c
  io_gpencil_import.c
  io_gpencil_utils.c
  io_obj.c
  io_ops.c
  io_ply_ops.c
  io_stl_ops.c
  io_usd.c

  io_alembic.h
  io_cache.h
  io_collada.h
  io_gpencil.h
  io_obj.h
  io_ops.h
  io_ply_ops.h
  io_stl_ops.h
  io_usd.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_OPENCOLLADA)
  list(APPEND LIB
    bf_collada
  )
  add_definitions(-DWITH_COLLADA)
endif()

if(WITH_IO_WAVEFRONT_OBJ)
  list(APPEND LIB
    bf_wavefront_obj
  )
  add_definitions(-DWITH_IO_WAVEFRONT_OBJ)
endif()

if(WITH_IO_PLY)
  list(APPEND LIB
    bf_ply
  )
  add_definitions(-DWITH_IO_PLY)
endif()

if(WITH_IO_STL)
  list(APPEND LIB
    bf_stl
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
    bf_alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

if(WITH_USD)
  list(APPEND LIB
    bf_usd
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
