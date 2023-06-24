# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS
)

set(SRC
  bvh.cpp
  bvh2.cpp
  binning.cpp
  build.cpp
  embree.cpp
  hiprt.cpp
  multi.cpp
  node.cpp
  optix.cpp
  sort.cpp
  split.cpp
  unaligned.cpp
)

set(SRC_METAL
  metal.mm
)

if(WITH_CYCLES_DEVICE_METAL)
  list(APPEND SRC
    ${SRC_METAL}
  )
  add_definitions(-DWITH_METAL)
endif()

set(SRC_HEADERS
  bvh.h
  bvh2.h
  binning.h
  build.h
  embree.h
  hiprt.h
  multi.h
  node.h
  optix.h
  params.h
  sort.h
  split.h
  unaligned.h
  metal.h
)

set(LIB
  cycles_scene
  cycles_util
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(WITH_CYCLES_EMBREE)
  list(APPEND LIB
    ${EMBREE_LIBRARIES}
  )
  if(EMBREE_SYCL_SUPPORT)
    list(APPEND LIB
      ${SYCL_LIBRARIES}
    )
  endif()
endif()

cycles_add_library(cycles_bvh "${LIB}" ${SRC} ${SRC_HEADERS})
