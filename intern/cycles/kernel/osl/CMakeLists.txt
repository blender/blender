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
  services_shared.h
  types.h
)

set(LIB
  PUBLIC cycles_scene

  PUBLIC bf::dependencies::optional::osl
  PRIVATE bf::dependencies::openimageio
  PRIVATE bf::dependencies::optional::pugixml
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_kernel_osl "${LIB}" ${SRC} ${HEADER_SRC})
