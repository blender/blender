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

#####################################################################
# Cycles standalone executable
#####################################################################

set(INC
  ..
)
set(INC_SYS
)

set(LIBRARIES
  cycles_device
  cycles_kernel
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_subd
  cycles_graph
  cycles_util
)

if(WITH_CYCLES_OSL)
  list(APPEND LIBRARIES cycles_kernel_osl)
endif()

if(CYCLES_STANDALONE_REPOSITORY)
  list(APPEND LIBRARIES extern_sky)
else()
  list(APPEND LIBRARIES bf_intern_sky)
endif()

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
  list(APPEND LIBRARIES ${GLUT_LIBRARIES})
endif()

list(APPEND LIBRARIES ${CYCLES_GL_LIBRARIES})

# Common configuration.

cycles_link_directories()

add_definitions(${GL_DEFINITIONS})

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

# Application build targets

if(WITH_CYCLES_STANDALONE)
  set(SRC
    cycles_standalone.cpp
    cycles_xml.cpp
    cycles_xml.h
    oiio_output_driver.cpp
    oiio_output_driver.h
  )
  add_executable(cycles ${SRC} ${INC} ${INC_SYS})
  unset(SRC)

  target_link_libraries(cycles ${LIBRARIES})
  cycles_target_link_libraries(cycles)

  if(APPLE)
    if(WITH_OPENCOLORIO)
      set_property(TARGET cycles APPEND_STRING PROPERTY LINK_FLAGS " -framework IOKit -framework Carbon")
    endif()
    if(WITH_OPENIMAGEDENOISE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
      # OpenImageDenoise uses BNNS from the Accelerate framework.
      set_property(TARGET cycles APPEND_STRING PROPERTY LINK_FLAGS " -framework Accelerate")
    endif()
  endif()

  if(UNIX AND NOT APPLE)
    set_target_properties(cycles PROPERTIES INSTALL_RPATH $ORIGIN/lib)
  endif()

  if(CYCLES_STANDALONE_REPOSITORY)
    cycles_install_libraries(cycles)
  endif()
endif()

#####################################################################
# Cycles cubin compiler executable
#####################################################################

if(WITH_CYCLES_CUBIN_COMPILER)
  # 32 bit windows is special, nvrtc is not supported on x86, so even
  # though we are building 32 bit blender a 64 bit cubin_cc will have
  # to be build to compile the cubins.
  if(MSVC AND NOT CMAKE_CL_64)
    message("Building with CUDA not supported on 32 bit, skipped")
    set(WITH_CYCLES_CUDA_BINARIES OFF CACHE BOOL "" FORCE)
  else()
    set(SRC
      cycles_cubin_cc.cpp
    )
    set(INC
      ../../../extern/cuew/include
    )
    add_executable(cycles_cubin_cc ${SRC})
    include_directories(${INC})
    cycles_target_link_libraries(cycles_cubin_cc)
    unset(SRC)
    unset(INC)
  endif()
endif()
