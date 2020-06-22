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
  ../draw
  ../imbuf
  ../makesdna
  ../makesrna

  ../editors/include

  # For node muting stuff...
  ../nodes
  ../nodes/intern

  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/mantaflow/extern
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  intern/gpu_attr_binding.c
  intern/gpu_batch.c
  intern/gpu_batch_presets.c
  intern/gpu_batch_utils.c
  intern/gpu_buffers.c
  intern/gpu_codegen.c
  intern/gpu_context.cpp
  intern/gpu_debug.c
  intern/gpu_draw.c
  intern/gpu_draw_smoke.c
  intern/gpu_element.c
  intern/gpu_extensions.c
  intern/gpu_framebuffer.c
  intern/gpu_immediate.c
  intern/gpu_immediate_util.c
  intern/gpu_init_exit.c
  intern/gpu_material.c
  intern/gpu_material_library.c
  intern/gpu_matrix.c
  intern/gpu_node_graph.c
  intern/gpu_platform.c
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

  GPU_attr_binding.h
  GPU_batch.h
  GPU_batch_presets.h
  GPU_batch_utils.h
  GPU_buffers.h
  GPU_common.h
  GPU_context.h
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
  GPU_platform.h
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
  intern/gpu_material_library.h
  intern/gpu_matrix_private.h
  intern/gpu_node_graph.h
  intern/gpu_primitive_private.h
  intern/gpu_private.h
  intern/gpu_select_private.h
  intern/gpu_shader_private.h
  intern/gpu_vertex_format_private.h
)

set(LIB
  ${BLENDER_GL_LIBRARIES}
)

if(NOT WITH_SYSTEM_GLEW)
  list(APPEND LIB
    ${BLENDER_GLEW_LIBRARIES}
  )
endif()

data_to_c_simple(shaders/gpu_shader_depth_only_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_checker_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_diag_stripes_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_simple_lighting_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_color_alpha_test_0_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_flat_id_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_area_borders_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_area_borders_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_base_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_base_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_shadow_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_widget_shadow_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_nodelink_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_nodelink_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_flat_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_line_dashed_uniform_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_line_dashed_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_smooth_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_smooth_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_rect_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_image_multi_rect_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_desaturate_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_overlays_merge_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_overlays_stereo_merge_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_shuffle_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_mask_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_modulate_alpha_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_alpha_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_image_varying_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_image_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_normal_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_flat_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_line_dashed_uniform_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_polyline_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_polyline_geom.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_polyline_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_smooth_color_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_smooth_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_passthrough_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_3D_clipped_uniform_color_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_instance_variying_size_variying_color_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_point_uniform_color_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_uniform_color_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_uniform_color_outline_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_varying_color_outline_aa_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_point_varying_color_varying_outline_aa_frag.glsl SRC)
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

data_to_c_simple(shaders/gpu_shader_2D_edituvs_points_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_edituvs_facedots_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_edituvs_edges_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_edituvs_edges_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_edituvs_faces_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_2D_edituvs_stretch_vert.glsl SRC)

data_to_c_simple(shaders/gpu_shader_text_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_text_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_keyframe_diamond_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_keyframe_diamond_frag.glsl SRC)

data_to_c_simple(shaders/gpu_shader_geometry.glsl SRC)

data_to_c_simple(shaders/material/gpu_shader_material_add_shader.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_ambient_occlusion.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_anisotropic.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_attribute.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_background.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_bevel.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_blackbody.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_bright_contrast.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_bump.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_camera.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_clamp.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_color_ramp.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_color_util.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_combine_hsv.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_combine_rgb.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_combine_xyz.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_diffuse.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_displacement.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_eevee_specular.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_emission.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_fractal_noise.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_fresnel.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_gamma.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_geometry.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_glass.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_glossy.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_hair_info.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_hash.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_holdout.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_hue_sat_val.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_invert.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_layer_weight.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_light_falloff.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_light_path.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_mapping.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_map_range.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_math.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_math_util.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_mix_rgb.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_mix_shader.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_noise.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_normal.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_normal_map.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_object_info.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_output_material.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_output_world.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_particle_info.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_principled.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_refraction.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_rgb_curves.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_rgb_to_bw.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_separate_hsv.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_separate_rgb.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_separate_xyz.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_set.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_shader_to_rgba.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_squeeze.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_subsurface_scattering.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tangent.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_brick.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_checker.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_environment.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_gradient.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_image.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_magic.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_musgrave.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_noise.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_sky.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_texture_coordinates.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_voronoi.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_wave.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_tex_white_noise.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_toon.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_translucent.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_transparent.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_uv_map.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_vector_curves.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_vector_displacement.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_vector_math.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_vector_rotate.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_velvet.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_vertex_color.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_volume_absorption.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_volume_info.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_volume_principled.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_volume_scatter.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_wireframe.glsl SRC)
data_to_c_simple(shaders/material/gpu_shader_material_world_normals.glsl SRC)

data_to_c_simple(shaders/gpu_shader_gpencil_stroke_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_stroke_frag.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_stroke_geom.glsl SRC)

data_to_c_simple(shaders/gpu_shader_gpencil_fill_vert.glsl SRC)
data_to_c_simple(shaders/gpu_shader_gpencil_fill_frag.glsl SRC)

data_to_c_simple(shaders/gpu_shader_cfg_world_clip_lib.glsl SRC)
data_to_c_simple(shaders/gpu_shader_colorspace_lib.glsl SRC)

data_to_c_simple(shaders/gpu_shader_common_obinfos_lib.glsl SRC)


if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

add_definitions(${GL_DEFINITIONS})

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
