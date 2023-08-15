# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
)

set(INC_SYS
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  eigen_capi.h

  intern/eigenvalues.cc
  intern/linear_solver.cc
  intern/matrix.cc
  intern/svd.cc

  intern/eigenvalues.h
  intern/linear_solver.h
  intern/matrix.h
  intern/svd.h
)

set(LIB
)

if(WITH_OPENMP_STATIC)
  list(APPEND LIB
    ${OpenMP_LIBRARIES}
  )
endif()

blender_add_lib(bf_intern_eigen "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
