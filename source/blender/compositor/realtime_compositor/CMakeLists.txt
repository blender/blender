# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  algorithms
  cached_resources
  ../../blenkernel
  ../../blentranslation
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesrna
  ../../nodes
  ../../render
  ../../gpu/intern

)

set(INC_SYS
)

set(SRC
  intern/compile_state.cc
  intern/context.cc
  intern/conversion_operation.cc
  intern/domain.cc
  intern/evaluator.cc
  intern/input_single_value_operation.cc
  intern/node_operation.cc
  intern/operation.cc
  intern/realize_on_domain_operation.cc
  intern/reduce_to_single_value_operation.cc
  intern/render_context.cc
  intern/result.cc
  intern/scheduler.cc
  intern/shader_node.cc
  intern/shader_operation.cc
  intern/simple_operation.cc
  intern/static_cache_manager.cc
  intern/texture_pool.cc
  intern/utilities.cc

  COM_compile_state.hh
  COM_context.hh
  COM_conversion_operation.hh
  COM_domain.hh
  COM_evaluator.hh
  COM_input_descriptor.hh
  COM_input_single_value_operation.hh
  COM_node_operation.hh
  COM_operation.hh
  COM_realize_on_domain_operation.hh
  COM_reduce_to_single_value_operation.hh
  COM_render_context.hh
  COM_result.hh
  COM_scheduler.hh
  COM_shader_node.hh
  COM_shader_operation.hh
  COM_simple_operation.hh
  COM_static_cache_manager.hh
  COM_texture_pool.hh
  COM_utilities.hh

  algorithms/intern/jump_flooding.cc
  algorithms/intern/morphological_blur.cc
  algorithms/intern/morphological_distance.cc
  algorithms/intern/morphological_distance_feather.cc
  algorithms/intern/parallel_reduction.cc
  algorithms/intern/realize_on_domain.cc
  algorithms/intern/smaa.cc
  algorithms/intern/summed_area_table.cc
  algorithms/intern/symmetric_separable_blur.cc
  algorithms/intern/symmetric_separable_blur_variable_size.cc
  algorithms/intern/transform.cc

  algorithms/COM_algorithm_jump_flooding.hh
  algorithms/COM_algorithm_morphological_blur.hh
  algorithms/COM_algorithm_morphological_distance.hh
  algorithms/COM_algorithm_morphological_distance_feather.hh
  algorithms/COM_algorithm_parallel_reduction.hh
  algorithms/COM_algorithm_realize_on_domain.hh
  algorithms/COM_algorithm_smaa.hh
  algorithms/COM_algorithm_summed_area_table.hh
  algorithms/COM_algorithm_symmetric_separable_blur.hh
  algorithms/COM_algorithm_symmetric_separable_blur_variable_size.hh
  algorithms/COM_algorithm_transform.hh

  cached_resources/intern/bokeh_kernel.cc
  cached_resources/intern/cached_image.cc
  cached_resources/intern/cached_mask.cc
  cached_resources/intern/cached_shader.cc
  cached_resources/intern/cached_texture.cc
  cached_resources/intern/distortion_grid.cc
  cached_resources/intern/keying_screen.cc
  cached_resources/intern/morphological_distance_feather_weights.cc
  cached_resources/intern/ocio_color_space_conversion_shader.cc
  cached_resources/intern/smaa_precomputed_textures.cc
  cached_resources/intern/symmetric_blur_weights.cc
  cached_resources/intern/symmetric_separable_blur_weights.cc

  cached_resources/COM_bokeh_kernel.hh
  cached_resources/COM_cached_image.hh
  cached_resources/COM_cached_mask.hh
  cached_resources/COM_cached_resource.hh
  cached_resources/COM_cached_shader.hh
  cached_resources/COM_cached_texture.hh
  cached_resources/COM_distortion_grid.hh
  cached_resources/COM_keying_screen.hh
  cached_resources/COM_morphological_distance_feather_weights.hh
  cached_resources/COM_ocio_color_space_conversion_shader.hh
  cached_resources/COM_smaa_precomputed_textures.hh
  cached_resources/COM_symmetric_blur_weights.hh
  cached_resources/COM_symmetric_separable_blur_weights.hh
)

set(LIB
  PRIVATE bf::dna
  bf_gpu
  PRIVATE bf::intern::guardedalloc
  bf_nodes
  bf_imbuf
  bf_render
  PRIVATE bf::blenlib
  bf_blenkernel
)

set(GLSL_SRC
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
  shaders/compositor_directional_blur.glsl
  shaders/compositor_displace.glsl
  shaders/compositor_double_edge_mask_compute_boundary.glsl
  shaders/compositor_double_edge_mask_compute_gradient.glsl
  shaders/compositor_edge_filter.glsl
  shaders/compositor_ellipse_mask.glsl
  shaders/compositor_filter.glsl
  shaders/compositor_flip.glsl
  shaders/compositor_glare_bloom_downsample.glsl
  shaders/compositor_glare_bloom_upsample.glsl
  shaders/compositor_glare_ghost_accumulate.glsl
  shaders/compositor_glare_ghost_base.glsl
  shaders/compositor_glare_highlights.glsl
  shaders/compositor_glare_mix.glsl
  shaders/compositor_glare_simple_star_anti_diagonal_pass.glsl
  shaders/compositor_glare_simple_star_diagonal_pass.glsl
  shaders/compositor_glare_simple_star_horizontal_pass.glsl
  shaders/compositor_glare_simple_star_vertical_pass.glsl
  shaders/compositor_glare_streaks_accumulate.glsl
  shaders/compositor_glare_streaks_filter.glsl
  shaders/compositor_id_mask.glsl
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
  shaders/compositor_map_uv_anisotropic.glsl
  shaders/compositor_map_uv_nearest_neighbour.glsl
  shaders/compositor_morphological_blur.glsl
  shaders/compositor_morphological_distance.glsl
  shaders/compositor_morphological_distance_feather.glsl
  shaders/compositor_morphological_distance_threshold.glsl
  shaders/compositor_morphological_step.glsl
  shaders/compositor_motion_blur.glsl
  shaders/compositor_motion_blur_max_velocity_dilate.glsl
  shaders/compositor_movie_distortion.glsl
  shaders/compositor_normalize.glsl
  shaders/compositor_parallel_reduction.glsl
  shaders/compositor_pixelate.glsl
  shaders/compositor_plane_deform.glsl
  shaders/compositor_plane_deform_mask.glsl
  shaders/compositor_plane_deform_motion_blur.glsl
  shaders/compositor_plane_deform_motion_blur_mask.glsl
  shaders/compositor_premultiply_alpha.glsl
  shaders/compositor_projector_lens_distortion.glsl
  shaders/compositor_read_input.glsl
  shaders/compositor_realize_on_domain.glsl
  shaders/compositor_screen_lens_distortion.glsl
  shaders/compositor_smaa_blending_weight_calculation.glsl
  shaders/compositor_smaa_edge_detection.glsl
  shaders/compositor_smaa_neighborhood_blending.glsl
  shaders/compositor_split.glsl
  shaders/compositor_summed_area_table_compute_complete_blocks.glsl
  shaders/compositor_summed_area_table_compute_complete_x_prologues.glsl
  shaders/compositor_summed_area_table_compute_complete_y_prologues.glsl
  shaders/compositor_summed_area_table_compute_incomplete_prologues.glsl
  shaders/compositor_sun_beams.glsl
  shaders/compositor_symmetric_blur.glsl
  shaders/compositor_symmetric_blur_variable_size.glsl
  shaders/compositor_symmetric_separable_blur.glsl
  shaders/compositor_symmetric_separable_blur_variable_size.glsl
  shaders/compositor_tone_map_photoreceptor.glsl
  shaders/compositor_tone_map_simple.glsl
  shaders/compositor_write_output.glsl
  shaders/compositor_z_combine_compute_mask.glsl
  shaders/compositor_z_combine_from_mask.glsl
  shaders/compositor_z_combine_simple.glsl

  shaders/library/gpu_shader_compositor_alpha_over.glsl
  shaders/library/gpu_shader_compositor_blur_common.glsl
  shaders/library/gpu_shader_compositor_bright_contrast.glsl
  shaders/library/gpu_shader_compositor_channel_matte.glsl
  shaders/library/gpu_shader_compositor_chroma_matte.glsl
  shaders/library/gpu_shader_compositor_color_balance.glsl
  shaders/library/gpu_shader_compositor_color_correction.glsl
  shaders/library/gpu_shader_compositor_color_matte.glsl
  shaders/library/gpu_shader_compositor_color_spill.glsl
  shaders/library/gpu_shader_compositor_color_to_luminance.glsl
  shaders/library/gpu_shader_compositor_difference_matte.glsl
  shaders/library/gpu_shader_compositor_distance_matte.glsl
  shaders/library/gpu_shader_compositor_exposure.glsl
  shaders/library/gpu_shader_compositor_gamma.glsl
  shaders/library/gpu_shader_compositor_hue_correct.glsl
  shaders/library/gpu_shader_compositor_hue_saturation_value.glsl
  shaders/library/gpu_shader_compositor_image_diagonals.glsl
  shaders/library/gpu_shader_compositor_invert.glsl
  shaders/library/gpu_shader_compositor_jump_flooding_lib.glsl
  shaders/library/gpu_shader_compositor_luminance_matte.glsl
  shaders/library/gpu_shader_compositor_main.glsl
  shaders/library/gpu_shader_compositor_map_value.glsl
  shaders/library/gpu_shader_compositor_motion_blur_lib.glsl
  shaders/library/gpu_shader_compositor_normal.glsl
  shaders/library/gpu_shader_compositor_ocio_processor.glsl
  shaders/library/gpu_shader_compositor_posterize.glsl
  shaders/library/gpu_shader_compositor_separate_combine.glsl
  shaders/library/gpu_shader_compositor_set_alpha.glsl
  shaders/library/gpu_shader_compositor_store_output.glsl
  shaders/library/gpu_shader_compositor_summed_area_table_lib.glsl
  shaders/library/gpu_shader_compositor_texture_utilities.glsl
  shaders/library/gpu_shader_compositor_type_conversion.glsl
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  data_to_c_simple(${GLSL_FILE} GLSL_C STRIP_LEADING_C_COMMENTS)
endforeach()

blender_add_lib(bf_compositor_shaders "${GLSL_C}" "" "" "")

list(APPEND LIB
  bf_compositor_shaders
)

set(GLSL_SOURCE_CONTENT "")
foreach(GLSL_FILE ${GLSL_SRC})
  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(datatoc_${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
endforeach()

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_compositor_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(bf_compositor_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

set(SRC_SHADER_CREATE_INFOS
  shaders/infos/compositor_alpha_crop_info.hh
  shaders/infos/compositor_bilateral_blur_info.hh
  shaders/infos/compositor_bokeh_blur_info.hh
  shaders/infos/compositor_bokeh_blur_variable_size_info.hh
  shaders/infos/compositor_bokeh_image_info.hh
  shaders/infos/compositor_box_mask_info.hh
  shaders/infos/compositor_compute_preview_info.hh
  shaders/infos/compositor_convert_info.hh
  shaders/infos/compositor_cryptomatte_info.hh
  shaders/infos/compositor_defocus_info.hh
  shaders/infos/compositor_despeckle_info.hh
  shaders/infos/compositor_directional_blur_info.hh
  shaders/infos/compositor_displace_info.hh
  shaders/infos/compositor_double_edge_mask_info.hh
  shaders/infos/compositor_edge_filter_info.hh
  shaders/infos/compositor_ellipse_mask_info.hh
  shaders/infos/compositor_filter_info.hh
  shaders/infos/compositor_flip_info.hh
  shaders/infos/compositor_glare_info.hh
  shaders/infos/compositor_id_mask_info.hh
  shaders/infos/compositor_image_crop_info.hh
  shaders/infos/compositor_inpaint_info.hh
  shaders/infos/compositor_jump_flooding_info.hh
  shaders/infos/compositor_keying_info.hh
  shaders/infos/compositor_keying_screen_info.hh
  shaders/infos/compositor_kuwahara_info.hh
  shaders/infos/compositor_map_uv_info.hh
  shaders/infos/compositor_morphological_blur_info.hh
  shaders/infos/compositor_morphological_distance_feather_info.hh
  shaders/infos/compositor_morphological_distance_info.hh
  shaders/infos/compositor_morphological_distance_threshold_info.hh
  shaders/infos/compositor_morphological_step_info.hh
  shaders/infos/compositor_motion_blur_info.hh
  shaders/infos/compositor_movie_distortion_info.hh
  shaders/infos/compositor_normalize_info.hh
  shaders/infos/compositor_parallel_reduction_info.hh
  shaders/infos/compositor_pixelate_info.hh
  shaders/infos/compositor_plane_deform_info.hh
  shaders/infos/compositor_premultiply_alpha_info.hh
  shaders/infos/compositor_projector_lens_distortion_info.hh
  shaders/infos/compositor_read_input_info.hh
  shaders/infos/compositor_realize_on_domain_info.hh
  shaders/infos/compositor_screen_lens_distortion_info.hh
  shaders/infos/compositor_smaa_info.hh
  shaders/infos/compositor_split_info.hh
  shaders/infos/compositor_summed_area_table_info.hh
  shaders/infos/compositor_sun_beams_info.hh
  shaders/infos/compositor_symmetric_blur_info.hh
  shaders/infos/compositor_symmetric_blur_variable_size_info.hh
  shaders/infos/compositor_symmetric_separable_blur_info.hh
  shaders/infos/compositor_symmetric_separable_blur_variable_size_info.hh
  shaders/infos/compositor_tone_map_photoreceptor_info.hh
  shaders/infos/compositor_tone_map_simple_info.hh
  shaders/infos/compositor_write_output_info.hh
  shaders/infos/compositor_z_combine_info.hh
)

set(SHADER_CREATE_INFOS_CONTENT "")
foreach(DESCRIPTOR_FILE ${SRC_SHADER_CREATE_INFOS})
  string(APPEND SHADER_CREATE_INFOS_CONTENT "#include \"${DESCRIPTOR_FILE}\"\n")
endforeach()

set(shader_create_info_list_file "${CMAKE_CURRENT_BINARY_DIR}/compositor_shader_create_info_list.hh")
file(GENERATE OUTPUT ${shader_create_info_list_file} CONTENT "${SHADER_CREATE_INFOS_CONTENT}")

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_OPENCOLORIO)
  add_definitions(
    -DWITH_OCIO
  )

  list(APPEND INC_SYS
    ${OPENCOLORIO_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${OPENCOLORIO_LIBRARIES}
  )
endif()

blender_add_lib(bf_realtime_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
