# SPDX-FileCopyrightText: 2014 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenkernel
  ../blenlib
  ../depsgraph
  ../functions
  ../imbuf
  ../makesrna
  ../nodes
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  intern/SIM_mass_spring.cpp
  intern/hair_volume.cpp
  intern/implicit_blender.c
  intern/implicit_eigen.cpp

  intern/ConstrainedConjugateGradient.h
  intern/eigen_utils.h
  intern/implicit.h

  SIM_mass_spring.h
)

set(LIB
  bf_blenkernel
  PRIVATE bf::dna
)

if(WITH_OPENMP_STATIC)
  list(APPEND LIB
    ${OpenMP_LIBRARIES}
  )
endif()

blender_add_lib(bf_simulation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
