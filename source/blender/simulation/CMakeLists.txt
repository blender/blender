# SPDX-FileCopyrightText: 2014 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenkernel
  ../depsgraph
  ../functions
  ../imbuf
  ../makesrna
  ../nodes
)

set(INC_SYS
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  intern/SIM_mass_spring.cc
  intern/hair_volume.cc
  intern/implicit_blender.cc
  intern/implicit_eigen.cc

  intern/ConstrainedConjugateGradient.h
  intern/eigen_utils.h
  intern/implicit.h

  SIM_mass_spring.h
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_OPENMP_STATIC)
  list(APPEND LIB
    ${OpenMP_LIBRARIES}
  )
endif()

blender_add_lib(bf_simulation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
