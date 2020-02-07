# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_c_flag(
    "-Wno-strict-prototypes"
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
