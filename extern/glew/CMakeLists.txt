# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_c_flag(
    "-Wno-strict-prototypes"
  )
endif()
if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "12.1"))
  add_c_flag(
    "-Wno-address"
  )
endif()

# MSVC's inliner is not having a happy time with glewIsSupported
# causing this to be one of the most expensive things to build
# in blender. Optimize for size rather than speed sidesteps this
# problem, more details at
# https://developercommunity.visualstudio.com/content/problem/732941/slow-compilation-of-glewc-for-visual-studio-2019-x.html

if(MSVC)
  add_c_flag("/Os")
endif()

set(INC
  include
)

set(INC_SYS

)

if(UNIX)
  list(APPEND INC_SYS
    ${X11_X11_INCLUDE_PATH}
  )
endif()

set(SRC
  src/glew.c

  include/GL/eglew.h
  include/GL/glew.h
  include/GL/glxew.h
  include/GL/wglew.h
)

set(LIB
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(extern_glew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
