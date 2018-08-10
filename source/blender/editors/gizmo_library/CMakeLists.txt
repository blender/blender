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
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenkernel
	../../blenlib
	../../blentranslation
	../../bmesh
	../../depsgraph
	../../gpu
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
	../../../../intern/eigen
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	gizmo_draw_utils.c
	gizmo_geometry.h
	gizmo_library_intern.h
	gizmo_library_presets.c
	gizmo_library_utils.c
	geometry/geom_arrow_gizmo.c
	geometry/geom_cube_gizmo.c
	geometry/geom_dial_gizmo.c
	gizmo_types/arrow2d_gizmo.c
	gizmo_types/arrow3d_gizmo.c
	gizmo_types/blank3d_gizmo.c
	gizmo_types/button2d_gizmo.c
	gizmo_types/cage2d_gizmo.c
	gizmo_types/cage3d_gizmo.c
	gizmo_types/dial3d_gizmo.c
	gizmo_types/grab3d_gizmo.c
	gizmo_types/primitive3d_gizmo.c
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_gizmo_library "${SRC}" "${INC}" "${INC_SYS}")
