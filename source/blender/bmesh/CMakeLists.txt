# $Id: CMakeLists.txt 31746 2010-09-04 05:31:25Z joeedh $
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
	../avi
	../blenlib
	../blenloader
	../blenkernel
	operators
	intern
	../editors/mesh
	../gpu
	../ikplugin
	../imbuf
	../makesdna
	../makesrna
	../modifiers
	../nodes
	../editors/include
	../render/extern/include
	../../../extern/glew/include
	../../../intern/audaspace/intern
	../../../intern/bsp/extern ../blenfont
	../../../intern/decimation/extern
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/iksolver/extern
	../../../intern/memutil
	../../../intern/opennl/extern
	../../../intern/smoke/extern
	../../../intern/mikktspace
    ../../../source/blender/windowmanager # XXX - BAD LEVEL CALL WM_api.h
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	operators/bmesh_dupeops.c
	operators/utils.c
	operators/subdivideop.c
	operators/connectops.c
	operators/removedoubles.c
	operators/mirror.c
	operators/primitiveops.c
	operators/join_triangles.c
	operators/edgesplitop.c
	operators/createops.c
	operators/dissolveops.c
	operators/subdivideop.h
	operators/triangulateop.c
	operators/mesh_conv.c
	operators/extrudeops.c
	bmesh_operators.h
	bmesh_iterators.h
	intern/bmesh_newcore.c
	intern/bmesh_interp.c
	intern/bmesh_iterators.c
	intern/bmesh_marking.c
	intern/bmesh_mesh.c
	intern/bmesh_mods.c
	intern/bmesh_structure.h
	intern/bmesh_construct.c
	intern/bmesh_walkers.c
	intern/bmesh_to_editmesh.c
	intern/bmesh_operators_private.h
	intern/editmesh_to_bmesh.c
	intern/bmesh_structure.c
	intern/bmesh_polygon.c
	intern/bmesh_queries.c
	intern/bmesh_opdefines.c
	intern/bmesh_filters.c
	intern/bmesh_eulers.c
	intern/bmesh_operators.c
	intern/bmesh_private.h
	bmesh_error.h
	bmesh_queries.h
	bmesh.h
	bmesh_marking.h
	bmesh_operator_api.h
	bmesh_filters.h
	bmesh_class.h
	bmesh_walkers.h
)

add_definitions(-DGLEW_STATIC)

if(WITH_LZO)
	list(APPEND INC ../../../extern/lzo/minilzo)
	add_definitions(-DWITH_LZO)
endif()

if(WITH_LZMA)
	list(APPEND INC ../../../extern/lzma)
	add_definitions(-DWITH_LZMA)
endif()

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
endif()

blender_add_lib(bf_bmesh "${SRC}" "${INC}")
