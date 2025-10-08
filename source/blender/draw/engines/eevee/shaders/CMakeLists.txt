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
  eevee_debug_irradiance_grid_vert.glsl
  eevee_debug_surfels_vert.glsl
  eevee_depth_of_field_scatter_vert.glsl
  eevee_display_lightprobe_planar_vert.glsl
  eevee_display_lightprobe_sphere_vert.glsl
  eevee_display_lightprobe_volume_vert.glsl
  eevee_geom_curves_vert.glsl
  eevee_geom_mesh_vert.glsl
  eevee_geom_pointcloud_vert.glsl
  eevee_geom_volume_vert.glsl
  eevee_geom_world_vert.glsl
  eevee_lookdev_display_vert.glsl
  eevee_shadow_page_tile_vert.glsl
  eevee_shadow_tag_usage_vert.glsl
)

set(SRC_GLSL_FRAG
  eevee_debug_gbuffer_frag.glsl
  eevee_debug_irradiance_grid_frag.glsl
  eevee_debug_surfels_frag.glsl
  eevee_deferred_aov_clear_frag.glsl
  eevee_deferred_capture_frag.glsl
  eevee_deferred_combine_frag.glsl
  eevee_deferred_light_frag.glsl
  eevee_deferred_planar_frag.glsl
  eevee_deferred_thickness_amend_frag.glsl
  eevee_deferred_tile_classify_frag.glsl
  eevee_depth_of_field_scatter_frag.glsl
  eevee_display_lightprobe_planar_frag.glsl
  eevee_display_lightprobe_sphere_frag.glsl
  eevee_display_lightprobe_volume_frag.glsl
  eevee_film_copy_frag.glsl
  eevee_film_frag.glsl
  eevee_hiz_debug_frag.glsl
  eevee_light_culling_debug_frag.glsl
  eevee_lookdev_display_frag.glsl
  eevee_occupancy_convert_frag.glsl
  eevee_renderpass_clear_frag.glsl
  eevee_shadow_debug_frag.glsl
  eevee_shadow_page_tile_frag.glsl
  eevee_shadow_tag_usage_frag.glsl
  eevee_surf_capture_frag.glsl
  eevee_surf_deferred_frag.glsl
  eevee_surf_depth_frag.glsl
  eevee_surf_forward_frag.glsl
  eevee_surf_hybrid_frag.glsl
  eevee_surf_occupancy_frag.glsl
  eevee_surf_shadow_frag.glsl
  eevee_surf_volume_frag.glsl
  eevee_surf_world_frag.glsl
  eevee_volume_resolve_frag.glsl
)

set(SRC_GLSL_COMP
  eevee_ambient_occlusion_pass_comp.glsl
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
  eevee_film_comp.glsl
  eevee_film_cryptomatte_post_comp.glsl
  eevee_film_pass_convert_comp.glsl
  eevee_hiz_update_comp.glsl
  eevee_horizon_denoise_comp.glsl
  eevee_horizon_resolve_comp.glsl
  eevee_horizon_scan_comp.glsl
  eevee_horizon_setup_comp.glsl
  eevee_light_culling_select_comp.glsl
  eevee_light_culling_sort_comp.glsl
  eevee_light_culling_tile_comp.glsl
  eevee_light_culling_zbin_comp.glsl
  eevee_light_shadow_setup_comp.glsl
  eevee_lightprobe_sphere_convolve_comp.glsl
  eevee_lightprobe_sphere_irradiance_comp.glsl
  eevee_lightprobe_sphere_remap_comp.glsl
  eevee_lightprobe_sphere_select_comp.glsl
  eevee_lightprobe_sphere_sunlight_comp.glsl
  eevee_lightprobe_volume_bounds_comp.glsl
  eevee_lightprobe_volume_load_comp.glsl
  eevee_lightprobe_volume_offset_comp.glsl
  eevee_lightprobe_volume_ray_comp.glsl
  eevee_lightprobe_volume_world_comp.glsl
  eevee_lut_comp.glsl
  eevee_motion_blur_dilate_comp.glsl
  eevee_motion_blur_flatten_comp.glsl
  eevee_motion_blur_gather_comp.glsl
  eevee_ray_denoise_bilateral_comp.glsl
  eevee_ray_denoise_spatial_comp.glsl
  eevee_ray_denoise_temporal_comp.glsl
  eevee_ray_generate_comp.glsl
  eevee_ray_tile_classify_comp.glsl
  eevee_ray_tile_compact_comp.glsl
  eevee_ray_trace_fallback_comp.glsl
  eevee_ray_trace_planar_comp.glsl
  eevee_ray_trace_screen_comp.glsl
  eevee_shadow_clipmap_clear_comp.glsl
  eevee_shadow_page_allocate_comp.glsl
  eevee_shadow_page_clear_comp.glsl
  eevee_shadow_page_defrag_comp.glsl
  eevee_shadow_page_free_comp.glsl
  eevee_shadow_page_mask_comp.glsl
  eevee_shadow_tag_update_comp.glsl
  eevee_shadow_tag_usage_comp.glsl
  eevee_shadow_tag_usage_surfels_comp.glsl
  eevee_shadow_tag_usage_volume_comp.glsl
  eevee_shadow_tilemap_amend_comp.glsl
  eevee_shadow_tilemap_bounds_comp.glsl
  eevee_shadow_tilemap_finalize_comp.glsl
  eevee_shadow_tilemap_init_comp.glsl
  eevee_shadow_tilemap_rendermap_comp.glsl
  eevee_shadow_visibility_comp.glsl
  eevee_subsurface_convolve_comp.glsl
  eevee_subsurface_setup_comp.glsl
  eevee_surfel_cluster_build_comp.glsl
  eevee_surfel_light_comp.glsl
  eevee_surfel_list_build_comp.glsl
  eevee_surfel_list_flatten_comp.glsl
  eevee_surfel_list_prefix_comp.glsl
  eevee_surfel_list_prepare_comp.glsl
  eevee_surfel_list_sort_comp.glsl
  eevee_surfel_ray_comp.glsl
  eevee_vertex_copy_comp.glsl
  eevee_volume_integration_comp.glsl
  eevee_volume_scatter_comp.glsl
)

set(SRC_GLSL_LIB
  eevee_ambient_occlusion_lib.glsl
  eevee_attributes_curves_lib.glsl
  eevee_attributes_mesh_lib.glsl
  eevee_attributes_pointcloud_lib.glsl
  eevee_attributes_volume_lib.glsl
  eevee_attributes_world_lib.glsl
  eevee_bxdf_diffuse_lib.glsl
  eevee_bxdf_lib.glsl
  eevee_bxdf_microfacet_lib.glsl
  eevee_camera_lib.glsl
  eevee_closure_lib.glsl
  eevee_colorspace_lib.glsl
  eevee_cryptomatte_lib.glsl
  eevee_depth_of_field_accumulator_lib.glsl
  eevee_depth_of_field_lib.glsl
  eevee_film_lib.glsl
  eevee_filter_lib.glsl
  # eevee_forward_lib.glsl
  eevee_gbuffer_lib.glsl
  eevee_horizon_scan_eval_lib.glsl
  eevee_horizon_scan_lib.glsl
  eevee_light_eval_lib.glsl
  eevee_light_iter_lib.glsl
  eevee_light_lib.glsl
  eevee_lightprobe_eval_lib.glsl
  eevee_lightprobe_lib.glsl
  eevee_lightprobe_sphere_eval_lib.glsl
  eevee_lightprobe_sphere_lib.glsl
  eevee_lightprobe_sphere_mapping_lib.glsl
  eevee_lightprobe_volume_eval_lib.glsl
  eevee_ltc_lib.glsl
  eevee_motion_blur_lib.glsl
  # eevee_nodetree_lib.glsl # Has dependency on draw_model_lib that is hard to resolve
  eevee_occupancy_lib.glsl
  eevee_octahedron_lib.glsl
  eevee_ray_generate_lib.glsl
  eevee_ray_trace_screen_lib.glsl
  eevee_ray_types_lib.glsl
  eevee_renderpass_lib.glsl
  eevee_reverse_z_lib.glsl
  eevee_sampling_lib.glsl
  eevee_shadow_lib.glsl
  eevee_shadow_page_ops_lib.glsl
  eevee_shadow_tag_usage_lib.glsl
  eevee_shadow_tilemap_lib.glsl
  eevee_shadow_tracing_lib.glsl
  eevee_spherical_harmonics_lib.glsl
  eevee_subsurface_lib.glsl
  eevee_surf_lib.glsl
  eevee_surfel_list_lib.glsl
  eevee_thickness_lib.glsl
  # eevee_transparency_lib.glsl # Require Fragment shader include
  # eevee_velocity_lib.glsl # Has dependency on draw_model_lib that is hard to resolve
  eevee_volume_lib.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()
  # TODO Remove
  add_definitions(-DUSE_GPU_SHADER_CREATE_INFO)

  compile_sources_as_cpp(eevee_cpp_shaders_vert "${SRC_GLSL_VERT}" "GPU_VERTEX_SHADER")
  compile_sources_as_cpp(eevee_cpp_shaders_frag "${SRC_GLSL_FRAG}" "GPU_FRAGMENT_SHADER")
  compile_sources_as_cpp(eevee_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  # compile_sources_as_cpp(eevee_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
