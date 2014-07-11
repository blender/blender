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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../uvedit
	../../blenfont
	../../blenkernel
	../../blenlib
	../../bmesh
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	editface.c
	editmesh_add.c
	editmesh_bevel.c
	editmesh_bisect.c
	editmesh_extrude.c
	editmesh_inset.c
	editmesh_intersect.c
	editmesh_knife.c
	editmesh_knife_project.c
	editmesh_loopcut.c
	editmesh_path.c
	editmesh_rip.c
	editmesh_rip_edge.c
	editmesh_select.c
	editmesh_tools.c
	editmesh_utils.c
	mesh_data.c
	mesh_ops.c
	meshtools.c

	mesh_intern.h
)

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)

	list(APPEND INC
		../../../../extern/recastnavigation
	)

	list(APPEND SRC
		mesh_navmesh.c
	)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
	add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_BULLET)
	add_definitions(-DWITH_BULLET)
endif()

blender_add_lib(bf_editor_mesh "${SRC}" "${INC}" "${INC_SYS}")
