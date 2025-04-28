# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..

  # For variadic macros
  ../../../../blenlib

  ../../../intern
  ../../../intern/shaders

  ../../../../gpu
  ../../../../gpu/intern
  ../../../../gpu/shaders
  ../../../../gpu/shaders/common
  ../../../../gpu/shaders/infos

  ../../gpencil
  ../../gpencil/shaders/infos

  ../../select
  ../../select/shaders
)

set(SRC_GLSL_VERT
  overlay_armature_dof_vert.glsl
  overlay_armature_envelope_outline_vert.glsl
  overlay_armature_envelope_solid_vert.glsl
  overlay_armature_shape_outline_vert.glsl
  overlay_armature_shape_solid_vert.glsl
  overlay_armature_shape_wire_vert.glsl
  overlay_armature_sphere_outline_vert.glsl
  overlay_armature_sphere_solid_vert.glsl
  overlay_armature_stick_vert.glsl
  overlay_armature_wire_vert.glsl
  overlay_clipbound_vert.glsl
  overlay_depth_only_curves_vert.glsl
  overlay_depth_only_gpencil_vert.glsl
  overlay_depth_only_mesh_conservative_vert.glsl
  overlay_depth_only_pointcloud_vert.glsl
  overlay_depth_only_vert.glsl
  overlay_edit_curve_handle_vert.glsl
  overlay_edit_curve_normals_vert.glsl
  overlay_edit_curve_point_vert.glsl
  overlay_edit_curve_wire_vert.glsl
  overlay_edit_curves_handle_vert.glsl
  overlay_edit_gpencil_canvas_vert.glsl
  overlay_edit_lattice_point_vert.glsl
  overlay_edit_lattice_wire_vert.glsl
  overlay_edit_pointcloud_vert.glsl
  overlay_edit_mesh_analysis_vert.glsl
  overlay_edit_mesh_depth_vert.glsl
  overlay_edit_mesh_edge_vert.glsl
  overlay_edit_mesh_facedot_vert.glsl
  overlay_edit_mesh_normal_vert.glsl
  overlay_edit_mesh_skin_root_vert.glsl
  overlay_edit_mesh_vert.glsl
  overlay_edit_particle_point_vert.glsl
  overlay_edit_particle_strand_vert.glsl
  overlay_edit_uv_edges_vert.glsl
  overlay_edit_uv_face_dots_vert.glsl
  overlay_edit_uv_faces_vert.glsl
  overlay_edit_uv_image_vert.glsl
  overlay_edit_uv_stretching_vert.glsl
  overlay_edit_uv_tiled_image_borders_vert.glsl
  overlay_edit_uv_verts_vert.glsl
  overlay_extra_groundline_vert.glsl
  overlay_extra_lightprobe_grid_vert.glsl
  overlay_extra_loose_point_vert.glsl
  overlay_extra_point_vert.glsl
  overlay_extra_vert.glsl
  overlay_extra_wire_vert.glsl
  overlay_facing_vert.glsl
  overlay_grid_vert.glsl
  overlay_image_vert.glsl
  overlay_motion_path_line_vert.glsl
  overlay_motion_path_point_vert.glsl
  overlay_outline_prepass_curves_vert.glsl
  overlay_outline_prepass_gpencil_vert.glsl
  overlay_outline_prepass_pointcloud_vert.glsl
  overlay_outline_prepass_vert.glsl
  overlay_outline_prepass_wire_vert.glsl
  overlay_paint_face_vert.glsl
  overlay_paint_point_vert.glsl
  overlay_paint_texture_vert.glsl
  overlay_paint_weight_vert.glsl
  overlay_paint_wire_vert.glsl
  overlay_particle_hair_vert.glsl
  overlay_particle_shape_vert.glsl
  overlay_particle_vert.glsl
  overlay_sculpt_curves_cage_vert.glsl
  overlay_sculpt_curves_selection_vert.glsl
  overlay_sculpt_mask_vert.glsl
  overlay_viewer_attribute_curve_vert.glsl
  overlay_viewer_attribute_curves_vert.glsl
  overlay_viewer_attribute_mesh_vert.glsl
  overlay_viewer_attribute_pointcloud_vert.glsl
  overlay_volume_gridlines_vert.glsl
  overlay_volume_velocity_vert.glsl
  overlay_wireframe_vert.glsl
)

set(SRC_GLSL_FRAG
  overlay_antialiasing_frag.glsl
  overlay_armature_dof_solid_frag.glsl
  overlay_armature_envelope_solid_frag.glsl
  overlay_armature_shape_solid_frag.glsl
  overlay_armature_shape_wire_frag.glsl
  overlay_armature_sphere_solid_frag.glsl
  overlay_armature_stick_frag.glsl
  overlay_armature_wire_frag.glsl
  overlay_background_frag.glsl
  overlay_depth_only_frag.glsl
  overlay_depth_only_gpencil_frag.glsl
  overlay_edit_mesh_analysis_frag.glsl
  overlay_edit_mesh_frag.glsl
  overlay_edit_uv_edges_frag.glsl
  overlay_edit_uv_image_mask_frag.glsl
  overlay_edit_uv_verts_frag.glsl
  overlay_extra_frag.glsl
  overlay_extra_loose_point_frag.glsl
  overlay_extra_wire_frag.glsl
  overlay_facing_frag.glsl
  overlay_grid_background_frag.glsl
  overlay_grid_frag.glsl
  overlay_image_frag.glsl
  overlay_motion_path_line_frag.glsl
  overlay_outline_detect_frag.glsl
  overlay_outline_prepass_frag.glsl
  overlay_outline_prepass_gpencil_frag.glsl
  overlay_paint_texture_frag.glsl
  overlay_paint_weight_frag.glsl
  overlay_particle_frag.glsl
  overlay_particle_shape_frag.glsl
  overlay_point_varying_color_frag.glsl
  overlay_point_varying_color_varying_outline_aa_frag.glsl
  overlay_sculpt_curves_selection_frag.glsl
  overlay_sculpt_mask_frag.glsl
  overlay_uniform_color_frag.glsl
  # TODO(fclem): rename to overlay_varying_color_frag.glsl
  overlay_varying_color.glsl
  overlay_viewer_attribute_frag.glsl
  overlay_wireframe_frag.glsl
  overlay_xray_fade_frag.glsl
)


set(SRC_GLSL_COMP
)

set(SRC_GLSL_LIB
  overlay_common_lib.glsl
  overlay_edit_mesh_common_lib.glsl
  # overlay_edit_mesh_lib.glsl # TODO(fclem) Contains reference from the shader interface.
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()
  # TODO Remove
  add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

  compile_sources_as_cpp(overlay_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(overlay_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  # compile_sources_as_cpp(overlay_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(overlay_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
