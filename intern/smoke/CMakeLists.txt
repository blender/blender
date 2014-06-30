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
#
# The Original Code is: all of this file.
#
# Contributor(s): Daniel Genrich
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	intern
	../memutil
)

set(INC_SYS
	${BULLET_INCLUDE_DIRS}
	${PNG_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/EIGENVALUE_HELPER.cpp
	intern/FLUID_3D.cpp
	intern/FLUID_3D_SOLVERS.cpp
	intern/FLUID_3D_STATIC.cpp
	intern/LU_HELPER.cpp
	intern/spectrum.cpp
	intern/SPHERE.cpp
	intern/WTURBULENCE.cpp
	intern/smoke_API.cpp

	extern/smoke_API.h
	intern/EIGENVALUE_HELPER.h
	intern/FFT_NOISE.h
	intern/FLUID_3D.h
	intern/IMAGE.h
	intern/INTERPOLATE.h
	intern/LU_HELPER.h
	intern/MERSENNETWISTER.h
	intern/OBSTACLE.h
	intern/spectrum.h
	intern/SPHERE.h
	intern/VEC3.h
	intern/WAVELET_NOISE.h
	intern/WTURBULENCE.h
	intern/tnt/jama_eig.h
	intern/tnt/jama_lu.h
	intern/tnt/tnt.h
	intern/tnt/tnt_array1d.h
	intern/tnt/tnt_array1d_utils.h
	intern/tnt/tnt_array2d.h
	intern/tnt/tnt_array2d_utils.h
	intern/tnt/tnt_array3d.h
	intern/tnt/tnt_array3d_utils.h
	intern/tnt/tnt_cmat.h
	intern/tnt/tnt_fortran_array1d.h
	intern/tnt/tnt_fortran_array1d_utils.h
	intern/tnt/tnt_fortran_array2d.h
	intern/tnt/tnt_fortran_array2d_utils.h
	intern/tnt/tnt_fortran_array3d.h
	intern/tnt/tnt_fortran_array3d_utils.h
	intern/tnt/tnt_i_refvec.h
	intern/tnt/tnt_math_utils.h
	intern/tnt/tnt_sparse_matrix_csr.h
	intern/tnt/tnt_stopwatch.h
	intern/tnt/tnt_subscript.h
	intern/tnt/tnt_vec.h
	intern/tnt/tnt_version.h
)

# quiet -Wundef
add_definitions(-DDDF_DEBUG=0)

if(WITH_OPENMP)
	add_definitions(-DPARALLEL=1)
else()
	add_definitions(-DPARALLEL=0)
endif()

if(WITH_FFTW3)
	add_definitions(-DWITH_FFTW3)
	list(APPEND INC_SYS
		${FFTW3_INCLUDE_DIRS}
	)
endif()

blender_add_lib(bf_intern_smoke "${SRC}" "${INC}" "${INC_SYS}")

