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
	intern
	../container
	../guardedalloc
	../memutil
	../moto/include
	../../extern/carve/include
)

set(INC_SYS

)

set(SRC
	intern/BOP_CarveInterface.cpp
	intern/BSP_CSGMesh.cpp
	intern/BSP_MeshPrimitives.cpp
	intern/CSG_BooleanOps.cpp

	extern/CSG_BooleanOps.h
	intern/BOP_Interface.h
	intern/BSP_CSGException.h
	intern/BSP_CSGMesh.h
	intern/BSP_CSGMesh_CFIterator.h
	intern/BSP_MeshPrimitives.h
)

if(WITH_BOOST)
	if(NOT MSVC)
		# Boost is setting as preferred collections library in the Carve code when using MSVC compiler
		add_definitions(
			-DHAVE_BOOST_UNORDERED_COLLECTIONS
		)
	endif()

	add_definitions(
		-DCARVE_SYSTEM_BOOST
	)

	list(APPEND INC_SYS
		${BOOST_INCLUDE_DIR}
	)
endif()

blender_add_lib(bf_intern_bsp "${SRC}" "${INC}" "${INC_SYS}")
