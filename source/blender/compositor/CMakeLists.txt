# SPDX-FileCopyrightText: 2011-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  algorithms
  cached_resources
  derived_resources
  utilities
  ../gpu/intern
  ../makesrna
  ../render/intern
  ../imbuf/opencolorio/intern/libocio
)

set(INC_SYS

)

set(SRC
  COM_compile_state.hh
  COM_compositor.hh
  COM_context.hh
  COM_conversion_operation.hh
  COM_derived_resources.hh
  COM_domain.hh
  COM_evaluator.hh
  COM_implicit_input_operation.hh
  COM_input_descriptor.hh
  COM_input_single_value_operation.hh
  COM_meta_data.hh
  COM_multi_function_procedure_operation.hh
  COM_node_operation.hh
  COM_operation.hh
  COM_pixel_operation.hh
  COM_profiler.hh
  COM_realize_on_domain_operation.hh
  COM_render_context.hh
  COM_result.hh
  COM_scheduler.hh
  COM_shader_node.hh
  COM_shader_operation.hh
  COM_simple_operation.hh
  COM_static_cache_manager.hh
  COM_utilities.hh

  intern/COM_compositor.cc
  intern/compile_state.cc
  intern/context.cc
  intern/conversion_operation.cc
  intern/domain.cc
  intern/evaluator.cc
  intern/implicit_input_operation.cc
  intern/input_single_value_operation.cc
  intern/meta_data.cc
  intern/multi_function_procedure_operation.cc
  intern/node_operation.cc
  intern/operation.cc
  intern/pixel_operation.cc
  intern/profiler.cc
  intern/realize_on_domain_operation.cc
  intern/render_context.cc
  intern/result.cc
  intern/scheduler.cc
  intern/shader_node.cc
  intern/shader_operation.cc
  intern/simple_operation.cc
  intern/static_cache_manager.cc
  intern/utilities.cc

  algorithms/intern/compute_preview.cc
  algorithms/intern/convolve.cc
  algorithms/intern/deriche_gaussian_blur.cc
  algorithms/intern/extract_alpha.cc
  algorithms/intern/jump_flooding.cc
  algorithms/intern/morphological_blur.cc
  algorithms/intern/morphological_distance.cc
  algorithms/intern/morphological_distance_feather.cc
  algorithms/intern/pad.cc
  algorithms/intern/parallel_reduction.cc
  algorithms/intern/recursive_gaussian_blur.cc
  algorithms/intern/sample_pixel.cc
  algorithms/intern/smaa.cc
  algorithms/intern/summed_area_table.cc
  algorithms/intern/symmetric_separable_blur.cc
  algorithms/intern/symmetric_separable_blur_variable_size.cc
  algorithms/intern/van_vliet_gaussian_blur.cc

  algorithms/COM_algorithm_compute_preview.hh
  algorithms/COM_algorithm_convolve.hh
  algorithms/COM_algorithm_deriche_gaussian_blur.hh
  algorithms/COM_algorithm_extract_alpha.hh
  algorithms/COM_algorithm_jump_flooding.hh
  algorithms/COM_algorithm_morphological_blur.hh
  algorithms/COM_algorithm_morphological_distance.hh
  algorithms/COM_algorithm_morphological_distance_feather.hh
  algorithms/COM_algorithm_pad.hh
  algorithms/COM_algorithm_parallel_reduction.hh
  algorithms/COM_algorithm_recursive_gaussian_blur.hh
  algorithms/COM_algorithm_sample_pixel.hh
  algorithms/COM_algorithm_smaa.hh
  algorithms/COM_algorithm_summed_area_table.hh
  algorithms/COM_algorithm_symmetric_separable_blur.hh
  algorithms/COM_algorithm_symmetric_separable_blur_variable_size.hh
  algorithms/COM_algorithm_van_vliet_gaussian_blur.hh

  cached_resources/intern/bokeh_kernel.cc
  cached_resources/intern/cached_image.cc
  cached_resources/intern/cached_mask.cc
  cached_resources/intern/cached_shader.cc
  cached_resources/intern/deriche_gaussian_coefficients.cc
  cached_resources/intern/distortion_grid.cc
  cached_resources/intern/fog_glow_kernel.cc
  cached_resources/intern/image_coordinates.cc
  cached_resources/intern/keying_screen.cc
  cached_resources/intern/morphological_distance_feather_weights.cc
  cached_resources/intern/ocio_color_space_conversion_shader.cc
  cached_resources/intern/smaa_precomputed_textures.cc
  cached_resources/intern/symmetric_blur_weights.cc
  cached_resources/intern/symmetric_separable_blur_weights.cc
  cached_resources/intern/van_vliet_gaussian_coefficients.cc

  cached_resources/COM_bokeh_kernel.hh
  cached_resources/COM_cached_image.hh
  cached_resources/COM_cached_mask.hh
  cached_resources/COM_cached_resource.hh
  cached_resources/COM_cached_shader.hh
  cached_resources/COM_deriche_gaussian_coefficients.hh
  cached_resources/COM_distortion_grid.hh
  cached_resources/COM_fog_glow_kernel.hh
  cached_resources/COM_image_coordinates.hh
  cached_resources/COM_keying_screen.hh
  cached_resources/COM_morphological_distance_feather_weights.hh
  cached_resources/COM_ocio_color_space_conversion_shader.hh
  cached_resources/COM_smaa_precomputed_textures.hh
  cached_resources/COM_symmetric_blur_weights.hh
  cached_resources/COM_symmetric_separable_blur_weights.hh
  cached_resources/COM_van_vliet_gaussian_coefficients.hh

  derived_resources/intern/denoised_auxiliary_pass.cc

  derived_resources/COM_denoised_auxiliary_pass.hh

  utilities/intern/gpu_material.cc
  utilities/intern/oidn.cc

  utilities/COM_utilities_diagonals.hh
  utilities/COM_utilities_gpu_material.hh
  utilities/COM_utilities_oidn.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blentranslation
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PRIVATE bf::dependencies::optional::opencolorio
)

set(GLSL_SRC
  shaders/infos/compositor_alpha_crop_infos.hh
  shaders/infos/compositor_bilateral_blur_infos.hh
  shaders/infos/compositor_bokeh_blur_infos.hh
  shaders/infos/compositor_bokeh_blur_variable_size_infos.hh
  shaders/infos/compositor_bokeh_image_infos.hh
  shaders/infos/compositor_box_mask_infos.hh
  shaders/infos/compositor_compute_preview_infos.hh
  shaders/infos/compositor_convert_infos.hh
  shaders/infos/compositor_cryptomatte_infos.hh
  shaders/infos/compositor_defocus_infos.hh
  shaders/infos/compositor_deriche_gaussian_blur_infos.hh
  shaders/infos/compositor_despeckle_infos.hh
  shaders/infos/compositor_directional_blur_infos.hh
  shaders/infos/compositor_displace_infos.hh
  shaders/infos/compositor_double_edge_mask_infos.hh
  shaders/infos/compositor_edge_filter_infos.hh
  shaders/infos/compositor_ellipse_mask_infos.hh
  shaders/infos/compositor_filter_infos.hh
  shaders/infos/compositor_flip_infos.hh
  shaders/infos/compositor_gamma_correct_infos.hh
  shaders/infos/compositor_glare_infos.hh
  shaders/infos/compositor_id_mask_infos.hh
  shaders/infos/compositor_image_coordinates_infos.hh
  shaders/infos/compositor_image_crop_infos.hh
  shaders/infos/compositor_inpaint_infos.hh
  shaders/infos/compositor_jump_flooding_infos.hh
  shaders/infos/compositor_keying_infos.hh
  shaders/infos/compositor_keying_screen_infos.hh
  shaders/infos/compositor_kuwahara_infos.hh
  shaders/infos/compositor_lens_distortion_infos.hh
  shaders/infos/compositor_map_uv_infos.hh
  shaders/infos/compositor_morphological_blur_infos.hh
  shaders/infos/compositor_morphological_distance_feather_infos.hh
  shaders/infos/compositor_morphological_distance_infos.hh
  shaders/infos/compositor_morphological_distance_threshold_infos.hh
  shaders/infos/compositor_morphological_step_infos.hh
  shaders/infos/compositor_motion_blur_infos.hh
  shaders/infos/compositor_movie_distortion_infos.hh
  shaders/infos/compositor_normalize_infos.hh
  shaders/infos/compositor_pad_infos.hh
  shaders/infos/compositor_parallel_reduction_infos.hh
  shaders/infos/compositor_pixelate_infos.hh
  shaders/infos/compositor_plane_deform_infos.hh
  shaders/infos/compositor_premultiply_alpha_infos.hh
  shaders/infos/compositor_read_input_infos.hh
  shaders/infos/compositor_realize_on_domain_infos.hh
  shaders/infos/compositor_sample_pixel_infos.hh
  shaders/infos/compositor_scale_variable_infos.hh
  shaders/infos/compositor_smaa_infos.hh
  shaders/infos/compositor_split_infos.hh
  shaders/infos/compositor_summed_area_table_infos.hh
  shaders/infos/compositor_symmetric_blur_infos.hh
  shaders/infos/compositor_symmetric_blur_variable_size_infos.hh
  shaders/infos/compositor_symmetric_separable_blur_infos.hh
  shaders/infos/compositor_symmetric_separable_blur_variable_size_infos.hh
  shaders/infos/compositor_tone_map_photoreceptor_infos.hh
  shaders/infos/compositor_tone_map_simple_infos.hh
  shaders/infos/compositor_translate_wrapped_infos.hh
  shaders/infos/compositor_van_vliet_gaussian_blur_infos.hh
  shaders/infos/compositor_write_output_infos.hh
  shaders/infos/compositor_z_combine_infos.hh

  shaders/compositor_alpha_crop.glsl
  shaders/compositor_bilateral_blur.glsl
  shaders/compositor_bokeh_blur.glsl
  shaders/compositor_bokeh_blur_variable_size.glsl
  shaders/compositor_bokeh_image.glsl
  shaders/compositor_box_mask.glsl
  shaders/compositor_compute_preview.glsl
  shaders/compositor_convert.glsl
  shaders/compositor_cryptomatte_image.glsl
  shaders/compositor_cryptomatte_matte.glsl
  shaders/compositor_cryptomatte_pick.glsl
  shaders/compositor_defocus_blur.glsl
  shaders/compositor_defocus_radius_from_depth.glsl
  shaders/compositor_defocus_radius_from_scale.glsl
  shaders/compositor_despeckle.glsl
  shaders/compositor_deriche_gaussian_blur.glsl
  shaders/compositor_deriche_gaussian_blur_sum.glsl
  shaders/compositor_directional_blur.glsl
  shaders/compositor_displace.glsl
  shaders/compositor_displace_anisotropic.glsl
  shaders/compositor_double_edge_mask_compute_boundary.glsl
  shaders/compositor_double_edge_mask_compute_gradient.glsl
  shaders/compositor_edge_filter.glsl
  shaders/compositor_ellipse_mask.glsl
  shaders/compositor_filter.glsl
  shaders/compositor_flip.glsl
  shaders/compositor_gamma_correct.glsl
  shaders/compositor_glare_bloom_downsample.glsl
  shaders/compositor_glare_bloom_upsample.glsl
  shaders/compositor_glare_ghost_accumulate.glsl
  shaders/compositor_glare_ghost_base.glsl
  shaders/compositor_glare_highlights.glsl
  shaders/compositor_glare_kernel_downsample.glsl
  shaders/compositor_glare_mix.glsl
  shaders/compositor_glare_simple_star_anti_diagonal_pass.glsl
  shaders/compositor_glare_simple_star_diagonal_pass.glsl
  shaders/compositor_glare_simple_star_horizontal_pass.glsl
  shaders/compositor_glare_simple_star_vertical_pass.glsl
  shaders/compositor_glare_streaks_accumulate.glsl
  shaders/compositor_glare_streaks_filter.glsl
  shaders/compositor_glare_sun_beams.glsl
  shaders/compositor_glare_write_glare_output.glsl
  shaders/compositor_glare_write_highlights_output.glsl
  shaders/compositor_horizontal_lens_distortion.glsl
  shaders/compositor_id_mask.glsl
  shaders/compositor_image_coordinates_normalized.glsl
  shaders/compositor_image_coordinates_pixel.glsl
  shaders/compositor_image_coordinates_uniform.glsl
  shaders/compositor_image_crop.glsl
  shaders/compositor_inpaint_compute_boundary.glsl
  shaders/compositor_inpaint_compute_region.glsl
  shaders/compositor_inpaint_fill_region.glsl
  shaders/compositor_jump_flooding.glsl
  shaders/compositor_keying_compute_image.glsl
  shaders/compositor_keying_compute_matte.glsl
  shaders/compositor_keying_extract_chroma.glsl
  shaders/compositor_keying_replace_chroma.glsl
  shaders/compositor_keying_screen.glsl
  shaders/compositor_keying_tweak_matte.glsl
  shaders/compositor_kuwahara_anisotropic.glsl
  shaders/compositor_kuwahara_anisotropic_compute_structure_tensor.glsl
  shaders/compositor_kuwahara_classic.glsl
  shaders/compositor_map_uv.glsl
  shaders/compositor_map_uv_anisotropic.glsl
  shaders/compositor_morphological_blur.glsl
  shaders/compositor_morphological_distance.glsl
  shaders/compositor_morphological_distance_feather.glsl
  shaders/compositor_morphological_distance_threshold.glsl
  shaders/compositor_morphological_step.glsl
  shaders/compositor_motion_blur.glsl
  shaders/compositor_motion_blur_max_velocity_dilate.glsl
  shaders/compositor_movie_distortion.glsl
  shaders/compositor_normalize.glsl
  shaders/compositor_pad.glsl
  shaders/compositor_parallel_reduction.glsl
  shaders/compositor_pixelate.glsl
  shaders/compositor_plane_deform_anisotropic.glsl
  shaders/compositor_plane_deform_mask.glsl
  shaders/compositor_plane_deform_motion_blur.glsl
  shaders/compositor_plane_deform_motion_blur_mask.glsl
  shaders/compositor_plane_deform.glsl
  shaders/compositor_premultiply_alpha.glsl
  shaders/compositor_radial_lens_distortion.glsl
  shaders/compositor_read_input.glsl
  shaders/compositor_realize_on_domain.glsl
  shaders/compositor_sample_pixel.glsl
  shaders/compositor_scale_variable.glsl
  shaders/compositor_smaa_blending_weight_calculation.glsl
  shaders/compositor_smaa_edge_detection.glsl
  shaders/compositor_smaa_neighborhood_blending.glsl
  shaders/compositor_split.glsl
  shaders/compositor_summed_area_table_compute_complete_blocks.glsl
  shaders/compositor_summed_area_table_compute_complete_x_prologues.glsl
  shaders/compositor_summed_area_table_compute_complete_y_prologues.glsl
  shaders/compositor_summed_area_table_compute_incomplete_prologues.glsl
  shaders/compositor_symmetric_blur.glsl
  shaders/compositor_symmetric_blur_variable_size.glsl
  shaders/compositor_symmetric_separable_blur.glsl
  shaders/compositor_symmetric_separable_blur_variable_size.glsl
  shaders/compositor_tone_map_photoreceptor.glsl
  shaders/compositor_tone_map_simple.glsl
  shaders/compositor_translate_wrapped.glsl
  shaders/compositor_van_vliet_gaussian_blur.glsl
  shaders/compositor_van_vliet_gaussian_blur_sum.glsl
  shaders/compositor_write_output.glsl
  shaders/compositor_z_combine_compute_mask.glsl
  shaders/compositor_z_combine_from_mask_depth.glsl
  shaders/compositor_z_combine_from_mask_image.glsl
  shaders/compositor_z_combine_simple_depth.glsl
  shaders/compositor_z_combine_simple_image.glsl

  shaders/library/gpu_shader_compositor_alpha_over.glsl
  shaders/library/gpu_shader_compositor_bright_contrast.glsl
  shaders/library/gpu_shader_compositor_channel_matte.glsl
  shaders/library/gpu_shader_compositor_chroma_matte.glsl
  shaders/library/gpu_shader_compositor_code_generation.glsl
  shaders/library/gpu_shader_compositor_color_balance.glsl
  shaders/library/gpu_shader_compositor_color_correction.glsl
  shaders/library/gpu_shader_compositor_color_matte.glsl
  shaders/library/gpu_shader_compositor_color_spill.glsl
  shaders/library/gpu_shader_compositor_color_to_luminance.glsl
  shaders/library/gpu_shader_compositor_convert_alpha.glsl
  shaders/library/gpu_shader_compositor_difference_matte.glsl
  shaders/library/gpu_shader_compositor_distance_matte.glsl
  shaders/library/gpu_shader_compositor_eval.glsl
  shaders/library/gpu_shader_compositor_exposure.glsl
  shaders/library/gpu_shader_compositor_hue_correct.glsl
  shaders/library/gpu_shader_compositor_hue_saturation_value.glsl
  shaders/library/gpu_shader_compositor_image_diagonals.glsl
  shaders/library/gpu_shader_compositor_invert.glsl
  shaders/library/gpu_shader_compositor_jump_flooding_lib.glsl
  shaders/library/gpu_shader_compositor_luminance_matte.glsl
  shaders/library/gpu_shader_compositor_main.glsl
  shaders/library/gpu_shader_compositor_map_value.glsl
  shaders/library/gpu_shader_compositor_motion_blur_lib.glsl
  shaders/library/gpu_shader_compositor_ocio_processor.glsl
  shaders/library/gpu_shader_compositor_posterize.glsl
  shaders/library/gpu_shader_compositor_separate_combine.glsl
  shaders/library/gpu_shader_compositor_set_alpha.glsl
  shaders/library/gpu_shader_compositor_set_value.glsl
  shaders/library/gpu_shader_compositor_store.glsl
  shaders/library/gpu_shader_compositor_store_output.glsl
  shaders/library/gpu_shader_compositor_summed_area_table_lib.glsl
  shaders/library/gpu_shader_compositor_texture_utilities.glsl
  shaders/library/gpu_shader_compositor_type_conversion.glsl
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  glsl_to_c(${GLSL_FILE} GLSL_C)
endforeach()

blender_add_lib(bf_compositor_shaders "${GLSL_C}" "" "" "")
blender_set_target_unity_build(bf_compositor_shaders 10)

list(APPEND LIB
  bf_compositor_shaders
)

set(GLSL_SOURCE_CONTENT "")
set(GLSL_METADATA_CONTENT "")
set(GLSL_INFOS_CONTENT "")
foreach(GLSL_FILE ${GLSL_SRC})
  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
  string(APPEND GLSL_METADATA_CONTENT "#include \"${GLSL_FILE}.hh\"\n")
  string(APPEND GLSL_INFOS_CONTENT "#include \"${GLSL_FILE}.info\"\n")
endforeach()

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_compositor_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
set(glsl_metadata_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_compositor_metadata_list.hh")
file(GENERATE OUTPUT ${glsl_metadata_list_file} CONTENT "${GLSL_METADATA_CONTENT}")
list(APPEND SRC ${glsl_metadata_list_file})
set(glsl_infos_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_compositor_infos_list.hh")
file(GENERATE OUTPUT ${glsl_infos_list_file} CONTENT "${GLSL_INFOS_CONTENT}")
list(APPEND SRC ${glsl_infos_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(bf_compositor_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  add_definitions(-DOIDN_STATIC_LIB)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
    ${TBB_LIBRARIES}
  )
endif()

if(WITH_FFTW3)
  list(APPEND INC_SYS
    ${FFTW3_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFTW3_LIBRARIES}
  )
  add_definitions(-DWITH_FFTW3)
endif()

blender_add_lib(bf_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(CXX_WARN_NO_SUGGEST_OVERRIDE)
  target_compile_options(bf_compositor PRIVATE "-Wsuggest-override")
endif()
