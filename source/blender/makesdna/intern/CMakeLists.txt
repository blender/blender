# $Id$
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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

include_directories(../../../../intern/guardedalloc ..)

# Build makesdna executable
set(SRC
	makesdna.c
	../../../../intern/guardedalloc/intern/mallocn.c
)

if(WIN32 AND NOT UNIX)
	list(APPEND SRC
		../../../../intern/guardedalloc/intern/mmap_win.c
	)
endif()

set(SRC_DNA_INC
	../DNA_ID.h
	../DNA_action_types.h
	../DNA_actuator_types.h
	../DNA_anim_types.h
	../DNA_armature_types.h
	../DNA_boid_types.h
	../DNA_brush_types.h
	../DNA_camera_types.h
	../DNA_cloth_types.h
	../DNA_color_types.h
	../DNA_constraint_types.h
	../DNA_controller_types.h
	../DNA_curve_types.h
	../DNA_customdata_types.h
	../DNA_documentation.h
	../DNA_effect_types.h
	../DNA_fileglobal_types.h
	../DNA_genfile.h
	../DNA_gpencil_types.h
	../DNA_group_types.h
	../DNA_image_types.h
	../DNA_ipo_types.h
	../DNA_key_types.h
	../DNA_lamp_types.h
	../DNA_lattice_types.h
	../DNA_listBase.h
	../DNA_material_types.h
	../DNA_mesh_types.h
	../DNA_meshdata_types.h
	../DNA_meta_types.h
	../DNA_modifier_types.h
	../DNA_nla_types.h
	../DNA_node_types.h
	../DNA_object_fluidsim.h
	../DNA_object_force.h
	../DNA_object_types.h
	../DNA_outliner_types.h
	../DNA_packedFile_types.h
	../DNA_particle_types.h
	../DNA_property_types.h
	../DNA_scene_types.h
	../DNA_screen_types.h
	../DNA_sdna_types.h
	../DNA_sensor_types.h
	../DNA_sequence_types.h
	../DNA_smoke_types.h
	../DNA_sound_types.h
	../DNA_space_types.h
	../DNA_text_types.h
	../DNA_texture_types.h
	../DNA_userdef_types.h
	../DNA_vec_types.h
	../DNA_vfont_types.h
	../DNA_view2d_types.h
	../DNA_view3d_types.h
	../DNA_windowmanager_types.h
	../DNA_world_types.h
)

add_executable(makesdna ${SRC} ${SRC_DNA_INC})

# Output dna.c
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dna.c
	COMMAND ${CMAKE_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/makesdna ${CMAKE_CURRENT_BINARY_DIR}/dna.c ${CMAKE_SOURCE_DIR}/source/blender/makesdna/
	DEPENDS makesdna
)

# Build bf_dna library
set(SRC
	dna_genfile.c
	dna.c
	${SRC_DNA_INC}
)

blenderlib(bf_dna "${SRC}" "${INC}")

message(STATUS "Configuring makesdna")
