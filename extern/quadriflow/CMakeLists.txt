# SPDX-FileCopyrightText: 2019 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
