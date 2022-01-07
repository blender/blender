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
  ../../glew-mx
)

set(INC_SYS
  ${GLEW_INCLUDE_DIR}
)

set(SRC
  aligned_malloc.cpp
  debug.cpp
  ies.cpp
  log.cpp
  math_cdf.cpp
  md5.cpp
  murmurhash.cpp
  path.cpp
  profiling.cpp
  string.cpp
  simd.cpp
  system.cpp
  task.cpp
  thread.cpp
  time.cpp
  transform.cpp
  windows.cpp
)

set(LIB
  ${TBB_LIBRARIES}
)

if(WITH_CYCLES_STANDALONE)
  if(WITH_CYCLES_STANDALONE_GUI)
    list(APPEND SRC
      view.cpp
    )
  endif()
endif()

set(SRC_HEADERS
  algorithm.h
  aligned_malloc.h
  args.h
  array.h
  atomic.h
  boundbox.h
  debug.h
  defines.h
  deque.h
  disjoint_set.h
  guarded_allocator.cpp
  foreach.h
  function.h
  guarded_allocator.h
  half.h
  hash.h
  ies.h
  image.h
  image_impl.h
  list.h
  log.h
  map.h
  math.h
  math_cdf.h
  math_fast.h
  math_intersect.h
  math_float2.h
  math_float3.h
  math_float4.h
  math_int2.h
  math_int3.h
  math_int4.h
  math_matrix.h
  md5.h
  murmurhash.h
  openimagedenoise.h
  opengl.h
  openvdb.h
  optimization.h
  param.h
  path.h
  profiling.h
  progress.h
  projection.h
  queue.h
  rect.h
  set.h
  simd.h
  avxf.h
  avxb.h
  avxi.h
  semaphore.h
  sseb.h
  ssef.h
  ssei.h
  stack_allocator.h
  static_assert.h
  stats.h
  string.h
  system.h
  task.h
  tbb.h
  texture.h
  thread.h
  time.h
  transform.h
  types.h
  types_float2.h
  types_float2_impl.h
  types_float3.h
  types_float3_impl.h
  types_float4.h
  types_float4_impl.h
  types_float8.h
  types_float8_impl.h
  types_int2.h
  types_int2_impl.h
  types_int3.h
  types_int3_impl.h
  types_int4.h
  types_int4_impl.h
  types_uchar2.h
  types_uchar2_impl.h
  types_uchar3.h
  types_uchar3_impl.h
  types_uchar4.h
  types_uchar4_impl.h
  types_uint2.h
  types_uint2_impl.h
  types_uint3.h
  types_uint3_impl.h
  types_uint4.h
  types_uint4_impl.h
  types_ushort4.h
  types_vector3.h
  types_vector3_impl.h
  unique_ptr.h
  vector.h
  version.h
  view.h
  windows.h
  xml.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_definitions(${GL_DEFINITIONS})

cycles_add_library(cycles_util "${LIB}" ${SRC} ${SRC_HEADERS})
