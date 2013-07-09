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
		Recast/Include
		Detour/Include
)

set(INC_SYS

)

set(SRC 
		recast-capi.cpp
		recast-capi.h


		Detour/Source/DetourCommon.cpp
		Detour/Source/DetourNode.cpp
		Detour/Source/DetourStatNavMesh.cpp
		Detour/Source/DetourStatNavMeshBuilder.cpp
		Detour/Source/DetourTileNavMesh.cpp
		Detour/Source/DetourTileNavMeshBuilder.cpp

		Detour/Include/DetourCommon.h
		Detour/Include/DetourNode.h
		Detour/Include/DetourStatNavMesh.h
		Detour/Include/DetourStatNavMeshBuilder.h
		Detour/Include/DetourTileNavMesh.h
		Detour/Include/DetourTileNavMeshBuilder.h

		Recast/Source/Recast.cpp
		Recast/Source/RecastAlloc.cpp
		Recast/Source/RecastArea.cpp
		Recast/Source/RecastContour.cpp
		Recast/Source/RecastFilter.cpp
		Recast/Source/RecastLayers.cpp
		Recast/Source/RecastMesh.cpp
		Recast/Source/RecastMeshDetail.cpp
		Recast/Source/RecastRasterization.cpp
		Recast/Source/RecastRegion.cpp

		Recast/Include/Recast.h
		Recast/Include/RecastAlloc.h
		Recast/Include/RecastAssert.h
)

blender_add_lib(extern_recastnavigation "${SRC}" "${INC}" "${INC_SYS}")
