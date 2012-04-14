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

set(SRC_DNA_INC
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_ID.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_action_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_actuator_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_anim_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_armature_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_boid_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_brush_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_camera_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_cloth_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_color_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_constraint_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_controller_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_curve_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_customdata_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_defs.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_documentation.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_dynamicpaint_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_effect_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_fileglobal_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_genfile.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_gpencil_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_group_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_image_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_ipo_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_key_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_lamp_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_lattice_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_listBase.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_material_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_mesh_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_meshdata_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_meta_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_modifier_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_nla_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_node_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_object_fluidsim.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_object_force.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_object_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_outliner_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_packedFile_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_particle_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_property_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_scene_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_screen_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_sdna_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_sensor_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_sequence_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_smoke_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_sound_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_space_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_speaker_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_text_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_texture_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_userdef_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_vec_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_vfont_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_view2d_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_view3d_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_windowmanager_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_world_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_movieclip_types.h
	${CMAKE_CURRENT_SOURCE_DIR}/makesdna/DNA_tracking_types.h
)

add_subdirectory(editors)
add_subdirectory(windowmanager)
add_subdirectory(blenkernel)
add_subdirectory(blenlib)
add_subdirectory(bmesh)
add_subdirectory(render)
add_subdirectory(blenfont)
add_subdirectory(blenloader)
add_subdirectory(blenpluginapi)
add_subdirectory(ikplugin)
add_subdirectory(gpu)
add_subdirectory(imbuf)
add_subdirectory(avi)
add_subdirectory(nodes)
add_subdirectory(modifiers)
add_subdirectory(makesdna)
add_subdirectory(makesrna)

if(WITH_IMAGE_OPENEXR)
	add_subdirectory(imbuf/intern/openexr)
endif()

if(WITH_IMAGE_DDS)
	add_subdirectory(imbuf/intern/dds)
endif()

if(WITH_IMAGE_CINEON)
	add_subdirectory(imbuf/intern/cineon)
endif()

if(WITH_CODEC_QUICKTIME)
	add_subdirectory(quicktime)
endif()

if(WITH_PYTHON)
	add_subdirectory(python)
endif()

if(WITH_OPENCOLLADA)
	add_subdirectory(collada)
endif()
