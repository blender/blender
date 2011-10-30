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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	include
)

set(INC_SYS

)

set(SRC
	intern/MT_Assert.cpp
	intern/MT_CmMatrix4x4.cpp
	intern/MT_Matrix3x3.cpp
	intern/MT_Matrix4x4.cpp
	intern/MT_Plane3.cpp
	intern/MT_Point3.cpp
	intern/MT_Quaternion.cpp
	intern/MT_Transform.cpp
	intern/MT_Vector2.cpp
	intern/MT_Vector3.cpp
	intern/MT_Vector4.cpp
	intern/MT_random.cpp

	include/GEN_List.h
	include/GEN_Map.h
	include/MT_CmMatrix4x4.h
	include/MT_Matrix3x3.h
	include/MT_Matrix4x4.h
	include/MT_MinMax.h
	include/MT_Optimize.h
	include/MT_Plane3.h
	include/MT_Point2.h
	include/MT_Point3.h
	include/MT_Quaternion.h
	include/MT_Scalar.h
	include/MT_Stream.h
	include/MT_Transform.h
	include/MT_Tuple2.h
	include/MT_Tuple3.h
	include/MT_Tuple4.h
	include/MT_Vector2.h
	include/MT_Vector3.h
	include/MT_Vector4.h
	include/MT_assert.h
	include/MT_random.h
	include/NM_Scalar.h

	include/MT_Matrix3x3.inl
	include/MT_Matrix4x4.inl
	include/MT_Plane3.inl
	include/MT_Point2.inl
	include/MT_Point3.inl
	include/MT_Quaternion.inl
	include/MT_Vector2.inl
	include/MT_Vector3.inl
	include/MT_Vector4.inl
)

blender_add_lib(bf_intern_moto "${SRC}" "${INC}" "${INC_SYS}")
