# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS
  ${ZSTD_INCLUDE_DIRS}
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
  system.cpp
  task.cpp
  thread.cpp
  time.cpp
  transform.cpp
  transform_avx2.cpp
  windows.cpp
)

set(LIB
  ${TBB_LIBRARIES}
  ${ZSTD_LIBRARIES}
)

set(SRC_HEADERS
  algorithm.h
  aligned_malloc.h
  args.h
  array.h
  atomic.h
  boundbox.h
  color.h
  debug.h
  defines.h
  deque.h
  disjoint_set.h
  guarded_allocator.cpp
  guarded_allocator.h
  guiding.h
  half.h
  hash.h
  ies.h
  image.h
  image_impl.h
  list.h
  log.h
  map.h
  math.h
  math_base.h
  math_cdf.h
  math_fast.h
  math_intersect.h
  math_float2.h
  math_float3.h
  math_float4.h
  math_float8.h
  math_int2.h
  math_int3.h
  math_int4.h
  math_int8.h
  md5.h
  murmurhash.h
  openimagedenoise.h
  openvdb.h
  optimization.h
  param.h
  path.h
  profiling.h
  progress.h
  projection.h
  projection_inverse.h
  queue.h
  rect.h
  set.h
  simd.h
  semaphore.h
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
  types_base.h
  types_float2.h
  types_float3.h
  types_float4.h
  types_float8.h
  types_int2.h
  types_int3.h
  types_int4.h
  types_int8.h
  types_spectrum.h
  types_uchar2.h
  types_uchar3.h
  types_uchar4.h
  types_uint2.h
  types_uint3.h
  types_uint4.h
  types_ushort4.h
  unique_ptr.h
  unique_ptr_vector.h
  vector.h
  version.h
  windows.h
  xml.h
)

if(CXX_HAS_AVX2)
  set_source_files_properties(transform_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_FLAGS}")
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_util "${LIB}" ${SRC} ${SRC_HEADERS})
