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

  # Otherwise we get warnings here that we cant fix in external projects
  remove_strict_flags()
endif()

macro(CYCLES_TEST SRC EXTRA_LIBS)
  if(WITH_GTESTS)
    BLENDER_SRC_GTEST("cycles_${SRC}" "${SRC}_test.cpp" "${EXTRA_LIBS}")
  endif()
endmacro()

set(INC
  .
  ..
  ../device
  ../graph
  ../kernel
  ../render
  ../util
)

set(ALL_CYCLES_LIBRARIES
  cycles_device
  cycles_kernel
  cycles_render
  cycles_bvh
  cycles_graph
  cycles_subd
  cycles_util
  extern_clew
  ${CYCLES_GL_LIBRARIES}
)
include_directories(${INC})

cycles_link_directories()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}")

CYCLES_TEST(render_graph_finalize "${ALL_CYCLES_LIBRARIES};bf_intern_numaapi")
cycles_target_link_libraries(cycles_render_graph_finalize_test)
CYCLES_TEST(util_aligned_malloc "cycles_util")
CYCLES_TEST(util_path "cycles_util;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
CYCLES_TEST(util_string "cycles_util;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
CYCLES_TEST(util_task "cycles_util;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES};bf_intern_numaapi")
CYCLES_TEST(util_time "cycles_util;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
set_source_files_properties(util_avxf_avx_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
CYCLES_TEST(util_avxf_avx "cycles_util;bf_intern_numaapi;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
set_source_files_properties(util_avxf_avx2_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
CYCLES_TEST(util_avxf_avx2 "cycles_util;bf_intern_numaapi;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
CYCLES_TEST(util_transform "cycles_util;${OPENIMAGEIO_LIBRARIES};${BOOST_LIBRARIES}")
