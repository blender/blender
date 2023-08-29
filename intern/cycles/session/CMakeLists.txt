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

cycles_add_library(cycles_session "${LIB}" ${SRC} ${SRC_HEADERS})
