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
	../blenkernel
	../blenlib
	../editors/include
	../makesdna
	../../../intern/guardedalloc
)

set(SRC
	operators/bmo_bevel.c
	operators/bmo_connect.c
	operators/bmo_create.c
	operators/bmo_dissolve.c
	operators/bmo_dupe.c
	operators/bmo_edgesplit.c
	operators/bmo_extrude.c
	operators/bmo_join_triangles.c
	operators/bmo_mesh_conv.c
	operators/bmo_mirror.c
	operators/bmo_primitive.c
	operators/bmo_removedoubles.c
	operators/bmo_subdivide.c
	operators/bmo_subdivide.h
	operators/bmo_triangulate.c
	operators/bmo_utils.c

	intern/bmesh_construct.c
	intern/bmesh_construct.h
	intern/bmesh_core.c
	intern/bmesh_core.h
	intern/bmesh_inline.c
	intern/bmesh_interp.c
	intern/bmesh_interp.h
	intern/bmesh_iterators.c
	intern/bmesh_iterators.h
	intern/bmesh_iterators_inline.c
	intern/bmesh_marking.c
	intern/bmesh_marking.h
	intern/bmesh_mesh.c
	intern/bmesh_mesh.h
	intern/bmesh_mods.c
	intern/bmesh_mods.h
	intern/bmesh_opdefines.c
	intern/bmesh_operator_api_inline.c
	intern/bmesh_operators.c
	intern/bmesh_operators.h
	intern/bmesh_operators_private.h
	intern/bmesh_polygon.c
	intern/bmesh_polygon.h
	intern/bmesh_private.h
	intern/bmesh_queries.c
	intern/bmesh_queries.h
	intern/bmesh_structure.c
	intern/bmesh_structure.h
	intern/bmesh_walkers.c
	intern/bmesh_walkers.h
	intern/bmesh_walkers_impl.c
	intern/bmesh_walkers_private.h

	tools/BME_bevel.c

	bmesh.h
	bmesh_class.h
	bmesh_error.h
	bmesh_operator_api.h
)

if(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
endif()

blender_add_lib(bf_bmesh "${SRC}" "${INC}" "${INC_SYS}")
