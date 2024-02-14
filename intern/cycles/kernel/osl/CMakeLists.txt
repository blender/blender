# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ../..
)

set(INC_SYS

)

set(SRC
  closures.cpp
  globals.cpp
  services.cpp
)

set(HEADER_SRC
  closures_setup.h
  closures_template.h
  compat.h
  globals.h
  osl.h
  services.h
  types.h
)

set(LIB
  cycles_scene

  ${OSL_LIBRARIES}
  ${OPENIMAGEIO_LIBRARIES}
  ${PUGIXML_LIBRARIES}
)

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
