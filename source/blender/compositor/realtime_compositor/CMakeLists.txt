# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  algorithms
  cached_resources
  ../../blenkernel
  ../../blenlib
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../render
  ../../gpu/intern
  ../../../../intern/guardedalloc
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
  intern/result.cc
  intern/scheduler.cc
  intern/shader_node.cc
  intern/shader_operation.cc
  intern/simple_operation.cc
  intern/static_cache_manager.cc
  intern/static_shader_manager.cc
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
  COM_result.hh
  COM_scheduler.hh
  COM_shader_node.hh
  COM_shader_operation.hh
  COM_simple_operation.hh
  COM_static_cache_manager.hh
  COM_static_shader_manager.hh
  COM_texture_pool.hh
  COM_utilities.hh

  algorithms/intern/algorithm_parallel_reduction.cc
  algorithms/intern/symmetric_separable_blur.cc

  algorithms/COM_algorithm_parallel_reduction.hh
  algorithms/COM_algorithm_symmetric_separable_blur.hh

  cached_resources/intern/morphological_distance_feather_weights.cc
  cached_resources/intern/symmetric_blur_weights.cc
  cached_resources/intern/symmetric_separable_blur_weights.cc

  cached_resources/COM_cached_resource.hh
  cached_resources/COM_morphological_distance_feather_weights.hh
  cached_resources/COM_symmetric_blur_weights.hh
  cached_resources/COM_symmetric_separable_blur_weights.hh
)

set(LIB
  bf_gpu
  bf_nodes
  bf_imbuf
  bf_render
  bf_blenlib
  bf_blenkernel
)

set(GLSL_SRC
  shaders/compositor_alpha_crop.glsl
  shaders/compositor_bilateral_blur.glsl
  shaders/compositor_blur.glsl
  shaders/compositor_blur_variable_size.glsl
  shaders/compositor_bokeh_image.glsl
  shaders/compositor_box_mask.glsl
  shaders/compositor_convert.glsl
  shaders/compositor_despeckle.glsl
  shaders/compositor_directional_blur.glsl
  shaders/compositor_edge_filter.glsl
  shaders/compositor_ellipse_mask.glsl
  shaders/compositor_filter.glsl
  shaders/compositor_flip.glsl
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
  shaders/compositor_image_crop.glsl
  shaders/compositor_morphological_distance.glsl
  shaders/compositor_morphological_distance_feather.glsl
  shaders/compositor_morphological_distance_threshold.glsl
  shaders/compositor_morphological_step.glsl
  shaders/compositor_normalize.glsl
  shaders/compositor_parallel_reduction.glsl
  shaders/compositor_projector_lens_distortion.glsl
  shaders/compositor_realize_on_domain.glsl
  shaders/compositor_screen_lens_distortion.glsl
  shaders/compositor_set_alpha.glsl
  shaders/compositor_split_viewer.glsl
  shaders/compositor_symmetric_blur.glsl
  shaders/compositor_symmetric_blur_variable_size.glsl
  shaders/compositor_symmetric_separable_blur.glsl
  shaders/compositor_tone_map_photoreceptor.glsl
  shaders/compositor_tone_map_simple.glsl

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
  shaders/library/gpu_shader_compositor_luminance_matte.glsl
  shaders/library/gpu_shader_compositor_main.glsl
  shaders/library/gpu_shader_compositor_map_value.glsl
  shaders/library/gpu_shader_compositor_normal.glsl
  shaders/library/gpu_shader_compositor_posterize.glsl
  shaders/library/gpu_shader_compositor_separate_combine.glsl
  shaders/library/gpu_shader_compositor_set_alpha.glsl
  shaders/library/gpu_shader_compositor_store_output.glsl
  shaders/library/gpu_shader_compositor_texture_utilities.glsl
  shaders/library/gpu_shader_compositor_type_conversion.glsl
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  data_to_c_simple(${GLSL_FILE} GLSL_C)
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
  shaders/infos/compositor_blur_info.hh
  shaders/infos/compositor_blur_variable_size_info.hh
  shaders/infos/compositor_bokeh_image_info.hh
  shaders/infos/compositor_box_mask_info.hh
  shaders/infos/compositor_convert_info.hh
  shaders/infos/compositor_despeckle_info.hh
  shaders/infos/compositor_directional_blur_info.hh
  shaders/infos/compositor_edge_filter_info.hh
  shaders/infos/compositor_ellipse_mask_info.hh
  shaders/infos/compositor_filter_info.hh
  shaders/infos/compositor_flip_info.hh
  shaders/infos/compositor_glare_info.hh
  shaders/infos/compositor_image_crop_info.hh
  shaders/infos/compositor_morphological_distance_feather_info.hh
  shaders/infos/compositor_morphological_distance_info.hh
  shaders/infos/compositor_morphological_distance_threshold_info.hh
  shaders/infos/compositor_morphological_step_info.hh
  shaders/infos/compositor_normalize_info.hh
  shaders/infos/compositor_parallel_reduction_info.hh
  shaders/infos/compositor_projector_lens_distortion_info.hh
  shaders/infos/compositor_realize_on_domain_info.hh
  shaders/infos/compositor_screen_lens_distortion_info.hh
  shaders/infos/compositor_set_alpha_info.hh
  shaders/infos/compositor_split_viewer_info.hh
  shaders/infos/compositor_symmetric_blur_info.hh
  shaders/infos/compositor_symmetric_blur_variable_size_info.hh
  shaders/infos/compositor_symmetric_separable_blur_info.hh
  shaders/infos/compositor_tone_map_photoreceptor_info.hh
  shaders/infos/compositor_tone_map_simple_info.hh
)

set(SHADER_CREATE_INFOS_CONTENT "")
foreach(DESCRIPTOR_FILE ${SRC_SHADER_CREATE_INFOS})
  string(APPEND SHADER_CREATE_INFOS_CONTENT "#include \"${DESCRIPTOR_FILE}\"\n")
endforeach()

set(shader_create_info_list_file "${CMAKE_CURRENT_BINARY_DIR}/compositor_shader_create_info_list.hh")
file(GENERATE OUTPUT ${shader_create_info_list_file} CONTENT "${SHADER_CREATE_INFOS_CONTENT}")

blender_add_lib(bf_realtime_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
