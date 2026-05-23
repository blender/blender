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

  # For grease pencil.
  ../../gpencil
)

set(SRC_GLSL_VERT
  eevee_depth_of_field_scatter_vert.glsl
)

set(SRC_GLSL_FRAG
  eevee_depth_of_field_scatter_frag.glsl
)

set(SRC_GLSL_COMP
  eevee_depth_of_field_bokeh_lut_comp.glsl
  eevee_depth_of_field_downsample_comp.glsl
  eevee_depth_of_field_filter_comp.glsl
  eevee_depth_of_field_gather_comp.glsl
  eevee_depth_of_field_hole_fill_comp.glsl
  eevee_depth_of_field_reduce_comp.glsl
  eevee_depth_of_field_resolve_comp.glsl
  eevee_depth_of_field_setup_comp.glsl
  eevee_depth_of_field_stabilize_comp.glsl
  eevee_depth_of_field_tiles_dilate_comp.glsl
  eevee_depth_of_field_tiles_flatten_comp.glsl
)

set(SRC_GLSL_LIB
  eevee_attributes_curves_lib.glsl
  eevee_attributes_mesh_lib.glsl
  eevee_attributes_pointcloud_lib.glsl
  eevee_attributes_volume_lib.glsl
  eevee_attributes_world_lib.glsl
  eevee_bxdf_diffuse_lib.glsl
  eevee_bxdf_lib.glsl
  eevee_bxdf_microfacet_lib.glsl
  eevee_closure_lib.glsl
  eevee_depth_of_field_accumulator_lib.glsl
  eevee_depth_of_field_lib.glsl
  eevee_film_lib.glsl
  eevee_filter_lib.glsl
  # eevee_forward_lib.glsl
  eevee_gbuffer_lib.glsl
  eevee_light_lib.glsl
  # eevee_nodetree_lib.glsl # Has dependency on draw_model_lib that is hard to resolve
  eevee_renderpass_lib.glsl
  eevee_sampling_lib.glsl
)

set(SRC_BSL
  bsl_shader_linting.cc
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  # TODO Remove
  add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

  compile_sources_as_cpp(eevee_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(eevee_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(eevee_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  compile_sources_as_cpp(eevee_cpp_shaders_bsl  "${SRC_BSL}" "")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(eevee_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
