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
	../../blenloader
	../../imbuf
	../../gpu
	../../makesdna
	../../makesrna
	../../nodes
	../../render/extern/include
	../../windowmanager
	../../compositor
	../../../../intern/guardedalloc
	../../../../intern/opennl/extern
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	drawnode.c
	node_buttons.c
	node_draw.c
	node_edit.c
	node_header.c
	node_ops.c
	node_select.c
	node_state.c
	node_templates.c
	space_node.c

	node_intern.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_COMPOSITOR)
	add_definitions(-DWITH_COMPOSITOR)
endif()

blender_add_lib(bf_editor_space_node "${SRC}" "${INC}" "${INC_SYS}")
