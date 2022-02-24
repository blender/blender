# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

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
endif()

cycles_add_library(cycles_bvh "${LIB}" ${SRC} ${SRC_HEADERS})
