# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS
)

set(SRC
  buffers.cpp
  cache_eviction.cpp
  denoising.cpp
  display_driver.cpp
  merge.cpp
  session.cpp
  tile.cpp
)

set(SRC_HEADERS
  buffers.h
  cache_eviction.h
  denoising.h
  display_driver.h
  merge.h
  output_driver.h
  session.h
  tile.h
)

set(LIB
  PUBLIC cycles_device
  PUBLIC cycles_integrator
  PUBLIC cycles_util
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_session "${LIB}" ${SRC} ${SRC_HEADERS})
