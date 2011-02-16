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

# Use PARENT_SCOPE so makesrna can access.
set(SRC_DNA_INC
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_ID.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_action_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_actuator_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_anim_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_armature_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_boid_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_brush_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_camera_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_cloth_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_color_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_constraint_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_controller_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_curve_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_customdata_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_documentation.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_effect_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_fileglobal_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_genfile.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_gpencil_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_group_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_image_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_ipo_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_key_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_lamp_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_lattice_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_listBase.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_material_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_mesh_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_meshdata_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_meta_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_modifier_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_nla_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_node_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_object_fluidsim.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_object_force.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_object_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_outliner_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_packedFile_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_particle_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_property_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_scene_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_screen_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_sdna_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_sensor_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_sequence_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_smoke_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_sound_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_space_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_text_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_texture_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_userdef_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_vec_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_vfont_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_view2d_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_view3d_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_windowmanager_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/DNA_world_types.h

	PARENT_SCOPE
)

add_subdirectory(intern)
