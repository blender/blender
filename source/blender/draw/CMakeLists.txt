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
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Blender Institute
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	engines/clay
	modes

	../blenkernel
	../blenlib
	../blentranslation
	../imbuf
	../depsgraph
	../makesdna
	../makesrna
	../gpu
	../editors/include
	../editors/space_view3d
	../render/extern/include
	../render/intern/include

	../../../intern/glew-mx
	../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/draw_manager.c
	intern/draw_mode_pass.c
	intern/draw_cache.c
	intern/draw_view.c
	intern/draw_armature.c
	engines/clay/clay.c
	modes/edit_armature_mode.c
	modes/edit_mesh_mode.c
	modes/object_mode.c

	intern/DRW_render.h
	intern/draw_mode_pass.h
	intern/draw_cache.h
	intern/draw_view.h
	engines/clay/clay.h
	modes/edit_armature_mode.h
	modes/edit_mesh_mode.h
	modes/object_mode.h

	./DRW_engine.h
)

if(WITH_CLAY_ENGINE)
	add_definitions(-DWITH_CLAY_ENGINE)
endif()

data_to_c_simple(engines/clay/shaders/clay_frag.glsl SRC)
data_to_c_simple(engines/clay/shaders/clay_vert.glsl SRC)
data_to_c_simple(engines/clay/shaders/ssao_alchemy.glsl SRC)
data_to_c_simple(engines/clay/shaders/ssao_groundtruth.glsl SRC)

data_to_c_simple(modes/shaders/edit_overlay_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_geom_tri.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_geom_edge.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_loosevert_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_facedot_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_overlay_facedot_vert.glsl SRC)

list(APPEND INC
)

blender_add_lib(bf_draw "${SRC}" "${INC}" "${INC_SYS}")
