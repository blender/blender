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
# The Original Code is Copyright (C) 2019, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_c_flag(
    "-Wno-unused-result"
  )
  remove_cc_flag(
    "-Wmissing-declarations"
  )
endif()

if(WIN32)
    add_definitions(-D_USE_MATH_DEFINES)
endif()

set(LEMON_3RD_PATH 3rd/lemon-1.3.1)

set(LEMON_SOURCE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${LEMON_3RD_PATH})

set(LEMON_SRC ${LEMON_SOURCE_ROOT_DIR}/lemon)
set(LEMON_INCLUDE_DIRS ${LEMON_SOURCE_ROOT_DIR})

set(LEMON_GEN_DIR ${CMAKE_BINARY_DIR}/extern/quadriflow/${LEMON_3RD_PATH})
configure_file(
  ${LEMON_SRC}/config.h.in
  ${LEMON_GEN_DIR}/lemon/config.h
)
set(LEMON_SOURCES
  ${LEMON_SRC}/arg_parser.cc
  ${LEMON_SRC}/base.cc
  ${LEMON_SRC}/color.cc
  ${LEMON_SRC}/lp_base.cc
  ${LEMON_SRC}/lp_skeleton.cc
  ${LEMON_SRC}/random.cc
  ${LEMON_SRC}/bits/windows.cc
)

set(INC
  src
  3rd/pcg32
  3rd/pss
  ${LEMON_INCLUDE_DIRS}
  ${LEMON_GEN_DIR}
)

set(INC_SYS
  ${BOOST_INCLUDE_DIR}
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  src/adjacent-matrix.cpp
  src/adjacent-matrix.hpp
  src/compare-key.hpp
  src/config.hpp
  src/dedge.cpp
  src/dedge.hpp
  src/disajoint-tree.hpp
  src/dset.hpp
  src/field-math.hpp
  src/flow.hpp
  src/hierarchy.cpp
  src/hierarchy.hpp
  src/loader.cpp
  src/loader.hpp
  src/localsat.cpp
  src/localsat.hpp
  src/merge-vertex.cpp
  src/merge-vertex.hpp
  src/optimizer.cpp
  src/optimizer.hpp
  src/parametrizer.cpp
  src/parametrizer-flip.cpp
  src/parametrizer-int.cpp
  src/parametrizer-mesh.cpp
  src/parametrizer-scale.cpp
  src/parametrizer-sing.cpp
  src/parametrizer.hpp
  src/serialize.hpp
  src/subdivide.cpp
  src/subdivide.hpp
  ${LEMON_SOURCES}
)

set(LIB
  ${BOOST_LIBRARIES}
)

blender_add_lib(extern_quadriflow "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
