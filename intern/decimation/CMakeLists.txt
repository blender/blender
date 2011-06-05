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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../container
	../guardedalloc
	../memutil
	../moto/include
)

set(INC_SYS

)

set(SRC
	intern/LOD_EdgeCollapser.cpp
	intern/LOD_ExternNormalEditor.cpp
	intern/LOD_FaceNormalEditor.cpp
	intern/LOD_ManMesh2.cpp
	intern/LOD_MeshPrimitives.cpp
	intern/LOD_QSDecimator.cpp
	intern/LOD_QuadricEditor.cpp
	intern/LOD_decimation.cpp

	extern/LOD_decimation.h
	intern/LOD_DecimationClass.h
	intern/LOD_EdgeCollapser.h
	intern/LOD_ExternBufferEditor.h
	intern/LOD_ExternNormalEditor.h
	intern/LOD_FaceNormalEditor.h
	intern/LOD_ManMesh2.h
	intern/LOD_MeshBounds.h
	intern/LOD_MeshException.h
	intern/LOD_MeshPrimitives.h
	intern/LOD_QSDecimator.h
	intern/LOD_Quadric.h
	intern/LOD_QuadricEditor.h
)

blender_add_lib(bf_intern_decimate "${SRC}" "${INC}" "${INC_SYS}")
