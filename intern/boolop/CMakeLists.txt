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

remove_strict_flags()

set(INC	
	.
	extern
	intern
	../container
	../guardedalloc
	../memutil
	../moto/include
	../../source/blender/blenlib
	../../source/blender/makesdna
)

set(INC_SYS

)

if(NOT WITH_CARVE)
	set(SRC
		intern/BOP_BBox.cpp
		intern/BOP_BSPNode.cpp
		intern/BOP_BSPTree.cpp
		intern/BOP_Edge.cpp
		intern/BOP_Face.cpp
		intern/BOP_Face2Face.cpp
		intern/BOP_Interface.cpp
		intern/BOP_MathUtils.cpp
		intern/BOP_Merge.cpp
		intern/BOP_Merge2.cpp
		intern/BOP_Mesh.cpp
		intern/BOP_Segment.cpp
		intern/BOP_Splitter.cpp
		intern/BOP_Tag.cpp
		intern/BOP_Triangulator.cpp
		intern/BOP_Vertex.cpp

		extern/BOP_Interface.h
		intern/BOP_BBox.h
		intern/BOP_BSPNode.h
		intern/BOP_BSPTree.h
		intern/BOP_Chrono.h
		intern/BOP_Edge.h
		intern/BOP_Face.h
		intern/BOP_Face2Face.h
		intern/BOP_Indexs.h
		intern/BOP_MathUtils.h
		intern/BOP_Merge.h
		intern/BOP_Merge2.h
		intern/BOP_Mesh.h
		intern/BOP_Misc.h
		intern/BOP_Segment.h
		intern/BOP_Splitter.h
		intern/BOP_Tag.h
		intern/BOP_Triangulator.h
		intern/BOP_Vertex.h
	)
else()
	set(SRC
		intern/BOP_CarveInterface.cpp
		extern/BOP_Interface.h
	)

	list(APPEND INC
		../../extern/carve/include
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

		list(APPEND INC
			${BOOST_INCLUDE_DIR}
		)
	endif()
endif()

blender_add_lib(bf_intern_bop "${SRC}" "${INC}" "${INC_SYS}")
