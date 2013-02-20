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
# The Original Code is Copyright (C) 2013, Blender Foundation
# All rights reserved.
#
# Contributor(s): Blender Foundation,
#                 Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../ldl/Include
	../../../colamd/Include
)

set(INC_SYS
)

set(SRC
	Geometry/v3d_metricbundle.cpp
	Math/v3d_optimization.cpp

	Geometry/v3d_cameramatrix.h
	Geometry/v3d_distortion.h
	Geometry/v3d_metricbundle.h
	Math/v3d_linear.h
	Math/v3d_linear_utils.h
	Math/v3d_mathutilities.h
	Math/v3d_optimization.h

	../ldl/Source/ldl.c
	../ldl/Include/ldl.h
)

if(WIN32)
	if(MSVC)
		set(MSVC_OFLAGS O1 O2 Ox)
		foreach(FLAG ${MSVC_OFLAGS})
			string(REPLACE "${FLAG}" "Od" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
			string(REPLACE "${FLAG}" "Od" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
		endforeach()
	endif()
endif()

add_definitions(
	-DV3DLIB_ENABLE_SUITESPARSE
)

blender_add_lib(extern_ssba "${SRC}" "${INC}" "${INC_SYS}")
