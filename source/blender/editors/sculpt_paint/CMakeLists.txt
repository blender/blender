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
	../../blenkernel
	../../blenlib
	../../blenfont
	../../bmesh
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	paint_cursor.c
	paint_curve.c
	paint_hide.c
	paint_image.c
	paint_image_2d.c
	paint_image_proj.c
	paint_mask.c
	paint_ops.c
	paint_stroke.c
	paint_undo.c
	paint_utils.c
	paint_vertex.c
	paint_vertex_proj.c
	sculpt.c
	sculpt_undo.c
	sculpt_uv.c

	paint_intern.h
	sculpt_intern.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}")
