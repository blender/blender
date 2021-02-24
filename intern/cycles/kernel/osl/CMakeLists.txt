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
  ../..
)

set(INC_SYS

)

set(SRC
  background.cpp
  bsdf_diffuse_ramp.cpp
  bsdf_phong_ramp.cpp
  emissive.cpp
  osl_bssrdf.cpp
  osl_closures.cpp
  osl_services.cpp
  osl_shader.cpp
)

set(HEADER_SRC
  osl_closures.h
  osl_globals.h
  osl_services.h
  osl_shader.h
)

set(LIB
  cycles_render

  ${OSL_LIBRARIES}
  ${OPENIMAGEIO_LIBRARIES}
  ${PUGIXML_LIBRARIES}
  ${CLANG_LIBRARIES}
  ${LLVM_LIBRARY}
)

# OSL and LLVM are built without RTTI
string(APPEND CMAKE_CXX_FLAGS " ${RTTI_DISABLE_FLAGS}")

if(APPLE)
  # Disable allocation warning on macOS prior to 10.14: the OSLRenderServices
  # contains member which is 64 bytes aligned (cache inside of OIIO's
  # unordered_map_concurrent). This is not something what the SDK supportsm, but
  # since we take care of allocations ourselves is is OK to ignore the
  # diagnostic message.
  string(APPEND CMAKE_CXX_FLAGS " -faligned-allocation")
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_kernel_osl "${LIB}" ${SRC} ${HEADER_SRC})
