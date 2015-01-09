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
	../../blenfont
	../../blenkernel
	../../blenlib
	../../bmesh
	../../gpu
	../../ikplugin
	../../imbuf
	../../makesdna
	../../makesrna
	../../python
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	object_add.c
	object_bake.c
	object_bake_api.c
	object_constraint.c
	object_edit.c
	object_group.c
	object_hook.c
	object_lattice.c
	object_lod.c
	object_modifier.c
	object_ops.c
	object_random.c
	object_relations.c
	object_select.c
	object_shapekey.c
	object_data_transfer.c
	object_transform.c
	object_warp.c
	object_vgroup.c

	object_intern.h
)

add_definitions(${GL_DEFINITIONS})

if(WITH_PYTHON)
	add_definitions(-DWITH_PYTHON)
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

blender_add_lib(bf_editor_object "${SRC}" "${INC}" "${INC_SYS}")
