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

# WITH_OPENGL limits the visibility of the opengl headers to just GPU and bg_gpu,
# to more easily highlight codepadths in other libraries that need to be refactored,
# bf_gpu is allowed to have opengl regardless of this option.

if(NOT WITH_OPENGL)
	add_definitions(-DWITH_OPENGL)
endif()

set(INC
	.
	../blenkernel
	../blenlib
	../bmesh
	../imbuf
	../makesdna
	../makesrna
	../draw

	../editors/include

	# For node muting stuff...
	../nodes
	../nodes/intern

	../../../intern/glew-mx
	../../../intern/guardedalloc
	../../../intern/smoke/extern
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/gpu_attr_binding.c
	intern/gpu_basic_shader.c
	intern/gpu_batch.c
	intern/gpu_batch_presets.c
	intern/gpu_batch_utils.c
	intern/gpu_buffers.c
	intern/gpu_codegen.c
	intern/gpu_context.cpp
	intern/gpu_debug.c
	intern/gpu_draw.c
	intern/gpu_element.c
	intern/gpu_extensions.c
	intern/gpu_framebuffer.c
	intern/gpu_immediate.c
	intern/gpu_immediate_util.c
	intern/gpu_init_exit.c
	intern/gpu_material.c
	intern/gpu_matrix.c
	intern/gpu_primitive.c
	intern/gpu_select.c
	intern/gpu_select_pick.c
	intern/gpu_select_sample_query.c
	intern/gpu_shader.c
	intern/gpu_shader_interface.c
	intern/gpu_state.c
	intern/gpu_texture.c
	intern/gpu_uniformbuffer.c
	intern/gpu_vertex_buffer.c
	intern/gpu_vertex_format.c
	intern/gpu_viewport.c

	shaders/gpu_shader_fx_lib.glsl
	shaders/gpu_shader_fx_ssao_frag.glsl
	shaders/gpu_shader_fx_dof_frag.glsl
	shaders/gpu_shader_fx_dof_vert.glsl
	shaders/gpu_shader_fx_dof_hq_frag.glsl
	shaders/gpu_shader_fx_dof_hq_vert.glsl
	shaders/gpu_shader_fx_dof_hq_geo.glsl
	shaders/gpu_shader_material.glsl
	shaders/gpu_shader_sep_gaussian_blur_frag.glsl
	shaders/gpu_shader_sep_gaussian_blur_vert.glsl
	shaders/gpu_shader_basic_frag.glsl
	shaders/gpu_shader_basic_vert.glsl
	shaders/gpu_shader_basic_geom.glsl
	shaders/gpu_shader_vertex.glsl
	shaders/gpu_shader_vsm_store_frag.glsl
	shaders/gpu_shader_vsm_store_vert.glsl
	shaders/gpu_shader_fx_depth_resolve.glsl
	shaders/gpu_shader_fire_frag.glsl
	shaders/gpu_shader_smoke_frag.glsl
	shaders/gpu_shader_smoke_vert.glsl

	GPU_attr_binding.h
	GPU_basic_shader.h
	GPU_batch.h
	GPU_buffers.h
	GPU_common.h
	GPU_debug.h
	GPU_draw.h
	GPU_element.h
	GPU_extensions.h
	GPU_framebuffer.h
	GPU_glew.h
	GPU_immediate.h
	GPU_immediate_util.h
	GPU_init_exit.h
	GPU_legacy_stubs.h
	GPU_material.h
	GPU_matrix.h
	GPU_primitive.h
	GPU_select.h
	GPU_shader.h
	GPU_shader_interface.h
	GPU_state.h
	GPU_texture.h
	GPU_uniformbuffer.h
	GPU_vertex_buffer.h
	GPU_vertex_format.h
	GPU_viewport.h

	intern/gpu_attr_binding_private.h
	intern/gpu_batch_private.h
	intern/gpu_codegen.h
	intern/gpu_context_private.h
	intern/gpu_primitive_private.h
	intern/gpu_private.h
	intern/gpu_select_private.h
	intern/gpu_shader_private.h
	intern/gpu_vertex_format_private.h
)

data_to_c_simple(shaders/gpu_shader_depth_only_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_checker_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_diag_stripes_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_lighting_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_lighting_smooth_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_lighting_smooth_color_alpha_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_color_alpha_test_0_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_id_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_base_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_base_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_shadow_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_shadow_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_nodelink_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_nodelink_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_flat_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_line_dashed_uniform_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_line_dashed_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_line_dashed_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_smooth_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_smooth_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_smooth_color_dithered_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_rect_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_multi_rect_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_desaturate_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_linear_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_shuffle_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_mask_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_modulate_alpha_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_alpha_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_varying_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_depth_linear_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_depth_copy_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_interlace_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_multisample_resolve_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_image_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_normal_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_flat_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_line_dashed_uniform_color_legacy_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_line_dashed_uniform_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_smooth_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_normal_smooth_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_smooth_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_passthrough_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_clipped_uniform_color_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_instance_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_variying_size_variying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_variying_size_variying_id_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_objectspace_variying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_screenspace_variying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_screen_aligned_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_camera_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_distance_line_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_edges_variying_color_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_instance_edges_variying_color_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_3D_groundline_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_groundpoint_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_point_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_uniform_color_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_uniform_color_outline_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_varying_color_outline_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_varying_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_point_fixed_size_varying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_point_varying_size_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_point_varying_size_varying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_point_uniform_size_aa_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_point_uniform_size_outline_aa_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_point_varying_size_varying_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_point_uniform_size_aa_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_point_uniform_size_varying_color_outline_aa_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_edges_front_back_persp_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_front_back_persp_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_front_back_persp_legacy_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_front_back_ortho_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_overlay_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_overlay_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_overlay_simple_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_edges_overlay_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_simple_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_simple_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_keyframe_diamond_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_keyframe_diamond_frag.glsl SRC)

data_to_c_simple(shaders/gpu_shader_geometry.glsl SRC)
data_to_c_simple(shaders/gpu_shader_fire_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_smoke_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_smoke_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_material.glsl SRC)
data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_sep_gaussian_blur_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_basic_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_basic_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_basic_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vertex.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vertex_world.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vsm_store_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_vsm_store_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_gpencil_stroke_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_stroke_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_stroke_geom.glsl SRC)

data_to_c_simple(shaders/gpu_shader_gpencil_fill_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_fill_frag.glsl SRC)


if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

add_definitions(${GL_DEFINITIONS})

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

if(WITH_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
endif()

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}")
