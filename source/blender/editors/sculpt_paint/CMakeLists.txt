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
	../../blenkernel
	../../blenlib
	../../blenloader
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../uvedit
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	paint_cursor.c
	paint_image.c
	paint_ops.c
	paint_stroke.c
	paint_undo.c
	paint_utils.c
	paint_vertex.c
	sculpt.c
	sculpt_undo.c
	sculpt_uv.c

	paint_intern.h
	sculpt_intern.h
)

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}")
