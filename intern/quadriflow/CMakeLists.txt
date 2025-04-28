# SPDX-FileCopyrightText: 2019 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
  ../../extern/quadriflow/src
)

set(SRC
  quadriflow_capi.cpp
  quadriflow_capi.hpp
)

set(LIB
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::dependencies::eigen
  extern_quadriflow
)

if(WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
endif()

blender_add_lib(bf_intern_quadriflow "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
