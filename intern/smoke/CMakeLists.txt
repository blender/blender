# $Id$
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

SET(INC
	intern
	../memutil
	../../extern/bullet2/src
	${PNG_INC}
	${ZLIB_INC}
)

SET(SRC
	intern/EIGENVALUE_HELPER.cpp
	intern/FLUID_3D.cpp
	intern/FLUID_3D_SOLVERS.cpp
	intern/FLUID_3D_STATIC.cpp
	intern/LU_HELPER.cpp
	intern/SPHERE.cpp
	intern/WTURBULENCE.cpp
	intern/smoke_API.cpp
)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(WITH_FFTW3)
	ADD_DEFINITIONS(-DFFTW3=1)
	LIST(APPEND INC ${FFTW3_INC})
ENDIF(WITH_FFTW3)

BLENDERLIB(bf_intern_smoke "${SRC}" "${INC}")

