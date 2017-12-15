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
	manipulator_draw_utils.c
	manipulator_geometry.h
	manipulator_library_intern.h
	manipulator_library_presets.c
	manipulator_library_utils.c
	geometry/geom_arrow_manipulator.c
	geometry/geom_cube_manipulator.c
	geometry/geom_dial_manipulator.c
	manipulator_types/arrow2d_manipulator.c
	manipulator_types/arrow3d_manipulator.c
	manipulator_types/button2d_manipulator.c
	manipulator_types/cage2d_manipulator.c
	manipulator_types/cage3d_manipulator.c
	manipulator_types/dial3d_manipulator.c
	manipulator_types/grab3d_manipulator.c
	manipulator_types/primitive3d_manipulator.c
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_manipulator_library "${SRC}" "${INC}" "${INC_SYS}")
