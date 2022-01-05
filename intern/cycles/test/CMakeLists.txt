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

if(WITH_GTESTS)
  Include(GTestTesting)

  # Otherwise we get warnings here that we can't fix in external projects
  remove_strict_flags()
endif()

set(INC
  ..
)

set(ALL_CYCLES_LIBRARIES
  cycles_kernel
  cycles_integrator
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_graph
  cycles_subd
  cycles_device
  cycles_util
  extern_clew
  ${CYCLES_GL_LIBRARIES}
)
include_directories(${INC})

cycles_link_directories()

set(SRC
  integrator_adaptive_sampling_test.cpp
  integrator_render_scheduler_test.cpp
  integrator_tile_test.cpp
  render_graph_finalize_test.cpp
  util_aligned_malloc_test.cpp
  util_math_test.cpp
  util_path_test.cpp
  util_string_test.cpp
  util_task_test.cpp
  util_time_test.cpp
  util_transform_test.cpp
)

# Disable AVX tests on macOS. Rosetta has problems running them, and other
# platforms should be enough to verify AVX operations are implemented correctly.
if(NOT APPLE)
  if(CXX_HAS_AVX)
    list(APPEND SRC
      util_avxf_avx_test.cpp
    )
    set_source_files_properties(util_avxf_avx_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
  endif()
  if(CXX_HAS_AVX2)
    list(APPEND SRC
      util_avxf_avx2_test.cpp
    )
    set_source_files_properties(util_avxf_avx2_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
  endif()
endif()

if(WITH_GTESTS)
  BLENDER_SRC_GTEST(cycles "${SRC}" "${ALL_CYCLES_LIBRARIES}")
  cycles_target_link_libraries(cycles_test)
endif()
