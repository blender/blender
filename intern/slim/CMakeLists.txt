# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Derived from `libigl`, a simple C++ geometry processing library.

set(INC
  .
  ../../source/blender/blenlib
  ../guardedalloc
)

set(INC_SYS
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  slim_matrix_transfer.h

  intern/area_compensation.cpp
  intern/area_compensation.h
  intern/cotmatrix.h
  intern/doublearea.h
  intern/edge_lengths.h
  intern/flip_avoiding_line_search.h
  intern/geometry_data_retrieval.cpp
  intern/geometry_data_retrieval.h
  intern/least_squares_relocator.cpp
  intern/least_squares_relocator.h
  intern/slim.cpp
  intern/slim.h
  intern/slim_matrix_transfer.cpp
  intern/slim_parametrizer.cpp
  intern/uv_initializer.cpp
  intern/uv_initializer.h
)

set(LIB
)

blender_add_lib(bf_intern_slim "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::intern::slim ALIAS bf_intern_slim)
