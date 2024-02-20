# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_IO_WAVEFRONT_OBJ OR WITH_IO_PLY OR WITH_IO_STL OR WITH_IO_GPENCIL OR WITH_ALEMBIC OR WITH_USD)
  add_subdirectory(common)
endif()

if(WITH_IO_WAVEFRONT_OBJ)
  add_subdirectory(wavefront_obj)
endif()

if(WITH_IO_PLY)
  add_subdirectory(ply)
endif()

if(WITH_IO_STL)
  add_subdirectory(stl)
endif()

if(WITH_IO_GPENCIL)
  add_subdirectory(gpencil)
endif()

if(WITH_ALEMBIC)
  add_subdirectory(alembic)
endif()

if(WITH_OPENCOLLADA)
  add_subdirectory(collada)
endif()

if(WITH_USD)
  add_subdirectory(usd)
endif()
