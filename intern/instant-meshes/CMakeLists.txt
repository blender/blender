# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../atomic
  ../eigen
  ../guardedalloc
  ../../extern/Eigen3
  ext/half
  ext/dset
  ext/pss
  ext/pcg32
  ../
  
)

set(INC_SYS
  ${GMP_INCLUDE_DIRS}
)

set(SRC
  src/aabb.h
  src/adjacency.cpp
  src/adjacency.h
  src/batch.cpp
  src/batch.h
  src/bvh.cpp
  src/bvh.h
  src/cleanup.cpp
  src/cleanup.h
  src/common.h
  src/dedge.cpp
  src/dedge.h
  src/diff.cpp
  src/extract.cpp
  src/extract.h
  src/field.cpp
  src/field.h
  src/meshstats.cpp
  src/meshstats.h
  src/normal.cpp
  src/normal.h
  src/hierarchy.cpp
  src/hierarchy.h
  src/reorder.cpp
  src/reorder.h
  src/subdivide.cpp
  src/subdivide.h
  src/smoothcurve.cpp
  src/smoothcurve.h
  src/c_api.cpp
  instant_meshes_c_api.h
)

set(LIB
)

if(WITH_TBB)
  add_definitions(-DWITH_TBB)

  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

if(WIN32 AND NOT UNIX)
  list(APPEND INC_SYS
    ${PTHREADS_INC}
  )

  list(APPEND LIB
    ${PTHREADS_LIBRARIES}
  )
endif()

blender_add_lib(bf_intern_instant_meshes "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
