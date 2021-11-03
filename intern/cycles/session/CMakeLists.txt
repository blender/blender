# Copyright 2011-2021 Blender Foundation
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

set(SRC
  buffers.cpp
  denoising.cpp
  merge.cpp
  session.cpp
  tile.cpp
)

set(SRC_HEADERS
  buffers.h
  display_driver.h
  denoising.h
  merge.h
  output_driver.h
  session.h
  tile.h
)

set(LIB
  cycles_device
  cycles_integrator
  cycles_util
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_definitions(${GL_DEFINITIONS})

cycles_add_library(cycles_session "${LIB}" ${SRC} ${SRC_HEADERS})
