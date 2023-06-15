# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
)

set(INC_SYS
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  intern/manifold_table.cpp
  intern/marching_cubes_table.cpp
  intern/octree.cpp
  intern/Projections.cpp

  intern/cubes.h
  intern/GeoCommon.h
  intern/manifold_table.h
  intern/marching_cubes_table.h
  intern/MemoryAllocator.h
  intern/ModelReader.h
  intern/octree.h
  intern/Projections.h
  intern/Queue.h

  intern/dualcon_c_api.cpp
  dualcon.h
)

set(LIB
)

blender_add_lib(bf_intern_dualcon "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
