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
	../../blentranslation
	../../bmesh
	../../draw
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../depsgraph
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
	../../../../intern/smoke/extern
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	drawobject.c
	drawvolume.c
	space_view3d.c
	view3d_buttons.c
	view3d_camera_control.c
	view3d_draw.c
	view3d_draw_legacy.c
	view3d_edit.c
	view3d_fly.c
	view3d_walk.c
	view3d_header.c
	view3d_iterators.c
	view3d_gizmo_armature.c
	view3d_gizmo_camera.c
	view3d_gizmo_empty.c
	view3d_gizmo_forcefield.c
	view3d_gizmo_lamp.c
	view3d_gizmo_navigate.c
	view3d_gizmo_navigate_type.c
	view3d_gizmo_ruler.c
	view3d_gizmo_preselect.c
	view3d_gizmo_preselect_type.c
	view3d_ops.c
	view3d_project.c
	view3d_ruler.c
	view3d_select.c
	view3d_snap.c
	view3d_toolbar.c
	view3d_utils.c
	view3d_view.c

	view3d_intern.h
)

if(WITH_PYTHON)
	blender_include_dirs(../../python)
	add_definitions(-DWITH_PYTHON)
endif()

add_definitions(${GL_DEFINITIONS})

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
	add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

blender_add_lib(bf_editor_space_view3d "${SRC}" "${INC}" "${INC_SYS}")
