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
# The Original Code is Copyright (C) 2015, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

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
