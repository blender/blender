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
# The Original Code is Copyright (C) 2011 Blender Foundation.
#
# Contributor(s): Blender Foundation,
#                 Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenkernel
	../../blenloader
	../../blenfont
	../../blenlib
	../../imbuf
	../../makesdna
	../../makesrna
	../../windowmanager
	../../gpu
	../../../../intern/guardedalloc
	${GLEW_INCLUDE_PATH}
)

set(INC_SYS
)

set(SRC
	space_clip.c
	clip_draw.c
	clip_toolbar.c
	clip_ops.c
	clip_graph_ops.c
	clip_graph_draw.c
	clip_editor.c
	clip_buttons.c
	clip_utils.c
	tracking_ops.c

	clip_intern.h
)

blender_add_lib(bf_editor_space_clip "${SRC}" "${INC}" "${INC_SYS}")
