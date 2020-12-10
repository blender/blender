# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(INC
  ..
)

set(INC_SYS
)

set(SRC
  bvh.cpp
  bvh2.cpp
  bvh_binning.cpp
  bvh_build.cpp
  bvh_embree.cpp
  bvh_multi.cpp
  bvh_node.cpp
  bvh_optix.cpp
  bvh_sort.cpp
  bvh_split.cpp
  bvh_unaligned.cpp
)

set(SRC_HEADERS
  bvh.h
  bvh2.h
  bvh_binning.h
  bvh_build.h
  bvh_embree.h
  bvh_multi.h
  bvh_node.h
  bvh_optix.h
  bvh_params.h
  bvh_sort.h
  bvh_split.h
  bvh_unaligned.h
)

set(LIB
  cycles_render
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
