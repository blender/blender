# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

if(WITH_GTESTS)
  # Otherwise we get warnings here that we can't fix in external projects
  remove_strict_flags()
endif()

set(INC
  ..
)

set(LIB
  cycles_kernel
  cycles_integrator
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_graph
  cycles_subd
  cycles_device
  cycles_util
)
cycles_external_libraries_append(LIB)

include_directories(${INC})

set(SRC
  integrator_adaptive_sampling_test.cpp
  integrator_render_scheduler_test.cpp
  integrator_tile_test.cpp
  kernel_camera_projection_test.cpp
  render_graph_finalize_test.cpp
  util_aligned_malloc_test.cpp
  util_boundbox_test.cpp
  util_ies_test.cpp
  util_math_test.cpp
  util_math_fast_test.cpp
  util_math_float3_test.cpp
  util_math_float4_test.cpp
  util_md5_test.cpp
  util_path_test.cpp
  util_string_test.cpp
  util_task_test.cpp
  util_time_test.cpp
  util_transform_test.cpp
)

# Disable AVX tests on macOS. Rosetta has problems running them, and other
# platforms should be enough to verify AVX operations are implemented correctly.
if(NOT APPLE)
  if(CXX_HAS_AVX2)
    list(APPEND SRC
      util_float8_avx2_test.cpp
    )
    set_source_files_properties(util_float8_avx2_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_FLAGS}")
  endif()
endif()

if(WITH_GTESTS)
  set(INC_SYS )
  blender_add_test_suite_executable(cycles "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
endif()
