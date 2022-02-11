# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

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
  bssrdf.cpp
  closures.cpp
  services.cpp
  shader.cpp
)

set(HEADER_SRC
  closures.h
  globals.h
  services.h
  shader.h
)

set(LIB
  cycles_scene

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
  # since we take care of allocations ourselves is OK to ignore the
  # diagnostic message.
  string(APPEND CMAKE_CXX_FLAGS " -faligned-allocation")
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_kernel_osl "${LIB}" ${SRC} ${HEADER_SRC})
