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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  intern
  ../blenkernel
  ../blenlib
  ../depsgraph
  ../functions
  ../imbuf
  ../makesdna
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
  intern/particle_allocator.cc
  intern/particle_function.cc
  intern/particle_mesh_emitter.cc
  intern/simulation_collect_influences.cc
  intern/simulation_solver_influences.cc
  intern/simulation_solver.cc
  intern/simulation_update.cc

  intern/ConstrainedConjugateGradient.h
  intern/eigen_utils.h
  intern/implicit.h
  intern/particle_allocator.hh
  intern/particle_function.hh
  intern/particle_mesh_emitter.hh
  intern/simulation_collect_influences.hh
  intern/simulation_solver_influences.hh
  intern/simulation_solver.hh
  intern/time_interval.hh

  SIM_mass_spring.h
  SIM_simulation_update.hh
)

set(LIB
  bf_blenkernel
  bf_nodes
)

if(WITH_OPENMP_STATIC)
  list(APPEND LIB
    ${OpenMP_LIBRARIES}
  )
endif()

blender_add_lib(bf_simulation "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
