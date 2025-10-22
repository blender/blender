# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC_GLSL
  .
  ..
  ./library
  ../../gpu
  ../../gpu/intern
  ../../gpu/shaders
  ../../gpu/shaders/common
  ../../gpu/shaders/infos
)

set(SRC_GLSL_COMP
# TODO: Port to C++ compilation
#   compositor_alpha_crop.glsl
#   compositor_bilateral_blur.glsl
#   compositor_bokeh_blur_variable_size.glsl
#   compositor_bokeh_blur.glsl
#   compositor_bokeh_image.glsl
#   compositor_box_mask.glsl
#   compositor_compute_preview.glsl
#   compositor_convert.glsl
#   compositor_cryptomatte_image.glsl
#   compositor_cryptomatte_matte.glsl
#   compositor_cryptomatte_pick.glsl
#   compositor_defocus_blur.glsl
#   compositor_defocus_radius_from_depth.glsl
#   compositor_defocus_radius_from_scale.glsl
#   compositor_deriche_gaussian_blur_sum.glsl
#   compositor_deriche_gaussian_blur.glsl
#   compositor_despeckle.glsl
#   compositor_directional_blur.glsl
#   compositor_displace_anisotropic.glsl
#   compositor_displace.glsl
#   compositor_double_edge_mask_compute_boundary.glsl
#   compositor_double_edge_mask_compute_gradient.glsl
#   compositor_edge_filter.glsl
#   compositor_ellipse_mask.glsl
#   compositor_filter.glsl
#   compositor_flip.glsl
#   compositor_gamma_correct.glsl
#   compositor_glare_bloom_downsample.glsl
#   compositor_glare_bloom_upsample.glsl
#   compositor_glare_ghost_accumulate.glsl
#   compositor_glare_ghost_base.glsl
#   compositor_glare_highlights.glsl
#   compositor_glare_mix.glsl
#   compositor_glare_simple_star_anti_diagonal_pass.glsl
#   compositor_glare_simple_star_diagonal_pass.glsl
#   compositor_glare_simple_star_horizontal_pass.glsl
#   compositor_glare_simple_star_vertical_pass.glsl
#   compositor_glare_streaks_accumulate.glsl
#   compositor_glare_streaks_filter.glsl
#   compositor_glare_sun_beams.glsl
#   compositor_glare_write_glare_output.glsl
#   compositor_glare_write_highlights_output.glsl
#   compositor_horizontal_lens_distortion.glsl
#   compositor_id_mask.glsl
#   compositor_image_coordinates_normalized.glsl
#   compositor_image_coordinates_pixel.glsl
#   compositor_image_coordinates_uniform.glsl
#   compositor_image_crop.glsl
#   compositor_inpaint_compute_boundary.glsl
#   compositor_inpaint_compute_region.glsl
#   compositor_inpaint_fill_region.glsl
#   compositor_jump_flooding.glsl
#   compositor_keying_compute_image.glsl
#   compositor_keying_compute_matte.glsl
#   compositor_keying_extract_chroma.glsl
#   compositor_keying_replace_chroma.glsl
#   compositor_keying_screen.glsl
#   compositor_keying_tweak_matte.glsl
#   compositor_kuwahara_anisotropic_compute_structure_tensor.glsl
#   compositor_kuwahara_anisotropic.glsl
#   compositor_kuwahara_classic.glsl
#   compositor_map_uv_anisotropic.glsl
#   compositor_map_uv.glsl
#   compositor_morphological_blur.glsl
#   compositor_morphological_distance_feather.glsl
#   compositor_morphological_distance_threshold.glsl
#   compositor_morphological_distance.glsl
#   compositor_morphological_step.glsl
#   compositor_motion_blur_max_velocity_dilate.glsl
#   compositor_motion_blur.glsl
#   compositor_movie_distortion.glsl
#   compositor_normalize.glsl
#   compositor_pad.glsl
  compositor_parallel_reduction.glsl
#   compositor_pixelate.glsl
#   compositor_plane_deform_anisotropic.glsl
#   compositor_plane_deform_mask.glsl
#   compositor_plane_deform_motion_blur_mask.glsl
#   compositor_plane_deform_motion_blur.glsl
#   compositor_plane_deform.glsl
#   compositor_premultiply_alpha.glsl
#   compositor_radial_lens_distortion.glsl
#   compositor_read_input.glsl
#   compositor_realize_on_domain.glsl
#   compositor_sample_pixel.glsl
#   compositor_scale_variable.glsl
#   compositor_smaa_blending_weight_calculation.glsl
#   compositor_smaa_edge_detection.glsl
#   compositor_smaa_neighborhood_blending.glsl
#   compositor_split.glsl
#   compositor_summed_area_table_compute_complete_blocks.glsl
#   compositor_summed_area_table_compute_complete_x_prologues.glsl
#   compositor_summed_area_table_compute_complete_y_prologues.glsl
#   compositor_summed_area_table_compute_incomplete_prologues.glsl
#   compositor_symmetric_blur_variable_size.glsl
#   compositor_symmetric_blur.glsl
#   compositor_symmetric_separable_blur_variable_size.glsl
#   compositor_symmetric_separable_blur.glsl
#   compositor_tone_map_photoreceptor.glsl
#   compositor_tone_map_simple.glsl
#   compositor_translate_wrapped.glsl
#   compositor_van_vliet_gaussian_blur_sum.glsl
#   compositor_van_vliet_gaussian_blur.glsl
#   compositor_write_output.glsl
#   compositor_z_combine_compute_mask.glsl
#   compositor_z_combine_from_mask_depth.glsl
#   compositor_z_combine_from_mask_image.glsl
#   compositor_z_combine_simple_depth.glsl
#   compositor_z_combine_simple_image.glsl
)

set(SRC_GLSL_LIB
  library/gpu_shader_compositor_texture_utilities.glsl
)

# Compile shaders with shader code.
if(WITH_GPU_SHADER_CPP_COMPILATION)
  with_shader_cpp_compilation_config()

  compile_sources_as_cpp(compositor_cpp_shaders_comp "${SRC_GLSL_COMP}" "GPU_COMPUTE_SHADER")
  # Only enable to make sure they compile on their own.
  # Otherwise it creates a warning about `pragma once`.
  compile_sources_as_cpp(compositor_cpp_shaders_lib "${SRC_GLSL_LIB}" "GPU_LIBRARY_SHADER")
endif()
