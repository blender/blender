# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  dummy
  intern
  metal
  opengl
  vulkan
  ../makesrna

  # For theme color access.
  ../editors/include

  # For `*_info.hh` includes.
  ../compositor/shaders/infos
  ../draw/engines/eevee
  ../draw/engines/eevee/shaders/infos
  ../draw/engines/gpencil
  ../draw/engines/gpencil/shaders/infos
  ../draw/engines/image/shaders/infos
  ../draw/engines/overlay/shaders/infos
  ../draw/engines/select
  ../draw/engines/select/shaders/infos
  ../draw/engines/workbench
  ../draw/engines/workbench/shaders/infos
  ../draw/intern
  ../draw/intern/shaders
  metal/kernels
  vulkan/shaders
  shaders/infos


  # For shader includes
  shaders/common
  shaders

  ../../../intern/ghost
  ../../../intern/mantaflow/extern
  ../../../intern/opensubdiv
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_RENDERDOC)
  list(APPEND INC
    ../../../extern/renderdoc/include
    ../../../intern/renderdoc_dynload/include
  )
  add_definitions(-DWITH_RENDERDOC)
endif()

if(WITH_GPU_SHADER_ASSERT)
  add_definitions(-DWITH_GPU_SHADER_ASSERT)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

set(INC_SYS
)

set(SRC
  intern/gpu_batch.cc
  intern/gpu_batch_presets.cc
  intern/gpu_batch_utils.cc
  intern/gpu_capabilities.cc
  intern/gpu_codegen.cc
  intern/gpu_compute.cc
  intern/gpu_context.cc
  intern/gpu_debug.cc
  intern/gpu_framebuffer.cc
  intern/gpu_immediate.cc
  intern/gpu_immediate_util.cc
  intern/gpu_index_buffer.cc
  intern/gpu_init_exit.cc
  intern/gpu_material.cc
  intern/gpu_matrix.cc
  intern/gpu_node_graph.cc
  intern/gpu_pass.cc
  intern/gpu_platform.cc
  intern/gpu_query.cc
  intern/gpu_select.cc
  intern/gpu_select_next.cc
  intern/gpu_select_pick.cc
  intern/gpu_select_sample_query.cc
  intern/gpu_shader.cc
  intern/gpu_shader_builtin.cc
  intern/gpu_shader_create_info.cc
  intern/gpu_shader_dependency.cc
  intern/gpu_shader_interface.cc
  intern/gpu_shader_log.cc
  intern/gpu_state.cc
  intern/gpu_storage_buffer.cc
  intern/gpu_texture.cc
  intern/gpu_texture_pool.cc
  intern/gpu_uniform_buffer.cc
  intern/gpu_vertex_buffer.cc
  intern/gpu_vertex_format.cc
  intern/gpu_vertex_format_normals.cc
  intern/gpu_viewport.cc
  intern/gpu_worker.cc

  GPU_attribute_convert.hh
  GPU_batch.hh
  GPU_batch_presets.hh
  GPU_batch_utils.hh
  GPU_capabilities.hh
  GPU_common.hh
  GPU_common_types.hh
  GPU_compilation_subprocess.hh
  GPU_compute.hh
  GPU_context.hh
  GPU_debug.hh
  GPU_format.hh
  GPU_framebuffer.hh
  GPU_immediate.hh
  GPU_immediate_util.hh
  GPU_index_buffer.hh
  GPU_init_exit.hh
  GPU_material.hh
  GPU_matrix.hh
  GPU_pass.hh
  GPU_platform.hh
  GPU_platform_backend_enum.h
  GPU_primitive.hh
  GPU_select.hh
  GPU_shader.hh
  GPU_shader_builtin.hh
  GPU_shader_shared.hh
  GPU_state.hh
  GPU_storage_buffer.hh
  GPU_texture.hh
  GPU_texture_pool.hh
  GPU_uniform_buffer.hh
  GPU_vertex_buffer.hh
  GPU_vertex_format.hh
  GPU_viewport.hh
  GPU_worker.hh

  intern/gpu_backend.hh
  intern/gpu_capabilities_private.hh
  intern/gpu_codegen.hh
  intern/gpu_context_private.hh
  intern/gpu_debug_private.hh
  intern/gpu_framebuffer_private.hh
  intern/gpu_immediate_private.hh
  intern/gpu_material_library.hh
  intern/gpu_matrix_private.hh
  intern/gpu_node_graph.hh
  intern/gpu_platform_private.hh
  intern/gpu_private.hh
  intern/gpu_profile_report.hh
  intern/gpu_query.hh
  intern/gpu_select_private.hh
  intern/gpu_shader_create_info.hh
  intern/gpu_shader_create_info_private.hh
  intern/gpu_shader_dependency_private.hh
  intern/gpu_shader_interface.hh
  intern/gpu_shader_private.hh
  intern/gpu_state_private.hh
  intern/gpu_storage_buffer_private.hh
  intern/gpu_texture_private.hh
  intern/gpu_uniform_buffer_private.hh
  intern/gpu_vertex_format_private.hh

  dummy/dummy_backend.hh
  dummy/dummy_batch.hh
  dummy/dummy_context.hh
  dummy/dummy_framebuffer.hh
  dummy/dummy_vertex_buffer.hh
)

set(OPENGL_SRC

  opengl/gl_backend.cc
  opengl/gl_batch.cc
  opengl/gl_compilation_subprocess.cc
  opengl/gl_compute.cc
  opengl/gl_context.cc
  opengl/gl_debug.cc
  opengl/gl_framebuffer.cc
  opengl/gl_immediate.cc
  opengl/gl_index_buffer.cc
  opengl/gl_query.cc
  opengl/gl_shader.cc
  opengl/gl_shader_interface.cc
  opengl/gl_shader_log.cc
  opengl/gl_state.cc
  opengl/gl_storage_buffer.cc
  opengl/gl_texture.cc
  opengl/gl_uniform_buffer.cc
  opengl/gl_vertex_array.cc
  opengl/gl_vertex_buffer.cc

  opengl/gl_backend.hh
  opengl/gl_batch.hh
  opengl/gl_compilation_subprocess.hh
  opengl/gl_compute.hh
  opengl/gl_context.hh
  opengl/gl_debug.hh
  opengl/gl_framebuffer.hh
  opengl/gl_immediate.hh
  opengl/gl_index_buffer.hh
  opengl/gl_primitive.hh
  opengl/gl_query.hh
  opengl/gl_shader.hh
  opengl/gl_shader_interface.hh
  opengl/gl_state.hh
  opengl/gl_storage_buffer.hh
  opengl/gl_texture.hh
  opengl/gl_uniform_buffer.hh
  opengl/gl_vertex_array.hh
  opengl/gl_vertex_buffer.hh
)

set(VULKAN_SRC
  vulkan/vk_backend.cc
  vulkan/vk_batch.cc
  vulkan/vk_buffer.cc
  vulkan/vk_common.cc
  vulkan/vk_context.cc
  vulkan/vk_data_conversion.cc
  vulkan/vk_debug.cc
  vulkan/vk_descriptor_pools.cc
  vulkan/vk_descriptor_set.cc
  vulkan/vk_descriptor_set_layouts.cc
  vulkan/vk_device.cc
  vulkan/vk_device_submission.cc
  vulkan/vk_fence.cc
  vulkan/vk_framebuffer.cc
  vulkan/vk_image_view.cc
  vulkan/vk_immediate.cc
  vulkan/vk_index_buffer.cc
  vulkan/vk_memory_layout.cc
  vulkan/vk_memory_pool.cc
  vulkan/vk_pipeline_pool.cc
  vulkan/vk_pixel_buffer.cc
  vulkan/vk_push_constants.cc
  vulkan/vk_query.cc
  vulkan/render_graph/nodes/vk_pipeline_data.cc
  vulkan/render_graph/vk_command_buffer_wrapper.cc
  vulkan/render_graph/vk_command_builder.cc
  vulkan/render_graph/vk_render_graph.cc
  vulkan/render_graph/vk_render_graph_links.cc
  vulkan/render_graph/vk_resource_access_info.cc
  vulkan/render_graph/vk_resource_state_tracker.cc
  vulkan/render_graph/vk_scheduler.cc
  vulkan/vk_resource_pool.cc
  vulkan/vk_sampler.cc
  vulkan/vk_samplers.cc
  vulkan/vk_shader.cc
  vulkan/vk_shader_compiler.cc
  vulkan/vk_shader_interface.cc
  vulkan/vk_shader_log.cc
  vulkan/vk_shader_module.cc
  vulkan/vk_staging_buffer.cc
  vulkan/vk_state_manager.cc
  vulkan/vk_storage_buffer.cc
  vulkan/vk_streaming_buffer.cc
  vulkan/vk_texture.cc
  vulkan/vk_to_string.cc
  vulkan/vk_uniform_buffer.cc
  vulkan/vk_vertex_attribute_object.cc
  vulkan/vk_vertex_buffer.cc

  vulkan/vk_backend.hh
  vulkan/vk_batch.hh
  vulkan/vk_buffer.hh
  vulkan/vk_common.hh
  vulkan/vk_context.hh
  vulkan/vk_data_conversion.hh
  vulkan/vk_debug.hh
  vulkan/vk_descriptor_pools.hh
  vulkan/vk_descriptor_set.hh
  vulkan/vk_descriptor_set_layouts.hh
  vulkan/vk_device.hh
  vulkan/vk_fence.hh
  vulkan/vk_framebuffer.hh
  vulkan/vk_ghost_api.hh
  vulkan/vk_image_view.hh
  vulkan/vk_immediate.hh
  vulkan/vk_index_buffer.hh
  vulkan/vk_memory.hh
  vulkan/vk_memory_layout.hh
  vulkan/vk_memory_pool.hh
  vulkan/vk_pipeline_pool.hh
  vulkan/vk_pixel_buffer.hh
  vulkan/vk_push_constants.hh
  vulkan/vk_query.hh
  vulkan/render_graph/nodes/vk_begin_query_node.hh
  vulkan/render_graph/nodes/vk_begin_rendering_node.hh
  vulkan/render_graph/nodes/vk_blit_image_node.hh
  vulkan/render_graph/nodes/vk_clear_attachments_node.hh
  vulkan/render_graph/nodes/vk_clear_color_image_node.hh
  vulkan/render_graph/nodes/vk_clear_depth_stencil_image_node.hh
  vulkan/render_graph/nodes/vk_copy_buffer_node.hh
  vulkan/render_graph/nodes/vk_copy_buffer_to_image_node.hh
  vulkan/render_graph/nodes/vk_copy_image_node.hh
  vulkan/render_graph/nodes/vk_copy_image_to_buffer_node.hh
  vulkan/render_graph/nodes/vk_dispatch_indirect_node.hh
  vulkan/render_graph/nodes/vk_dispatch_node.hh
  vulkan/render_graph/nodes/vk_draw_indexed_indirect_node.hh
  vulkan/render_graph/nodes/vk_draw_indexed_node.hh
  vulkan/render_graph/nodes/vk_draw_indirect_node.hh
  vulkan/render_graph/nodes/vk_draw_node.hh
  vulkan/render_graph/nodes/vk_end_query_node.hh
  vulkan/render_graph/nodes/vk_end_rendering_node.hh
  vulkan/render_graph/nodes/vk_fill_buffer_node.hh
  vulkan/render_graph/nodes/vk_node_info.hh
  vulkan/render_graph/nodes/vk_pipeline_data.hh
  vulkan/render_graph/nodes/vk_reset_query_pool_node.hh
  vulkan/render_graph/nodes/vk_synchronization_node.hh
  vulkan/render_graph/nodes/vk_update_buffer_node.hh
  vulkan/render_graph/nodes/vk_update_mipmaps_node.hh
  vulkan/render_graph/vk_command_buffer_wrapper.hh
  vulkan/render_graph/vk_command_builder.hh
  vulkan/render_graph/vk_render_graph.hh
  vulkan/render_graph/vk_render_graph_links.hh
  vulkan/render_graph/vk_render_graph_node.hh
  vulkan/render_graph/vk_resource_access_info.hh
  vulkan/render_graph/vk_resource_state_tracker.hh
  vulkan/render_graph/vk_scheduler.hh
  vulkan/vk_resource_pool.hh
  vulkan/vk_sampler.hh
  vulkan/vk_samplers.hh
  vulkan/vk_shader.hh
  vulkan/vk_shader_compiler.hh
  vulkan/vk_shader_interface.hh
  vulkan/vk_shader_log.hh
  vulkan/vk_shader_module.hh
  vulkan/vk_staging_buffer.hh
  vulkan/vk_state_manager.hh
  vulkan/vk_storage_buffer.hh
  vulkan/vk_streaming_buffer.hh
  vulkan/vk_texture.hh
  vulkan/vk_to_string.hh
  vulkan/vk_uniform_buffer.hh
  vulkan/vk_vertex_attribute_object.hh
  vulkan/vk_vertex_buffer.hh
)

set(METAL_SRC
  metal/mtl_backend.mm
  metal/mtl_batch.mm
  metal/mtl_command_buffer.mm
  metal/mtl_context.mm
  metal/mtl_debug.mm
  metal/mtl_framebuffer.mm
  metal/mtl_immediate.mm
  metal/mtl_index_buffer.mm
  metal/mtl_memory.mm
  metal/mtl_push_constant.cc
  metal/mtl_query.mm
  metal/mtl_shader.mm
  metal/mtl_shader_generate.cc
  metal/mtl_shader_interface.mm
  metal/mtl_shader_log.mm
  metal/mtl_state.mm
  metal/mtl_storage_buffer.mm
  metal/mtl_texture.mm
  metal/mtl_texture_util.mm
  metal/mtl_uniform_buffer.mm
  metal/mtl_vertex_buffer.mm

  metal/mtl_backend.hh
  metal/mtl_batch.hh
  metal/mtl_capabilities.hh
  metal/mtl_common.hh
  metal/mtl_command_buffer.hh
  metal/mtl_context.hh
  metal/mtl_debug.hh
  metal/mtl_framebuffer.hh
  metal/mtl_immediate.hh
  metal/mtl_index_buffer.hh
  metal/mtl_memory.hh
  metal/mtl_primitive.hh
  metal/mtl_pso_descriptor_state.hh
  metal/mtl_push_constant.hh
  metal/mtl_query.hh
  metal/mtl_shader.hh
  metal/mtl_shader_generate.hh
  metal/mtl_shader_interface.hh
  metal/mtl_shader_interface_type.hh
  metal/mtl_shader_log.hh
  metal/mtl_shader_shared.hh
  metal/mtl_state.hh
  metal/mtl_storage_buffer.hh
  metal/mtl_texture.hh
  metal/mtl_uniform_buffer.hh
  metal/mtl_vertex_buffer.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::imbuf
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::nodes
  PRIVATE bf::dependencies::optional::opencolorio
)

# Select Backend source based on availability
if(WITH_OPENGL_BACKEND)
  list(APPEND INC_SYS
    ${Epoxy_INCLUDE_DIRS}
  )
  list(APPEND SRC
    ${OPENGL_SRC}
  )
  list(APPEND LIB
    ${Epoxy_LIBRARIES}
  )
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_METAL_BACKEND)
  list(APPEND SRC ${METAL_SRC})
endif()


if(WITH_VULKAN_BACKEND)
  list(APPEND INC
    ../../../extern/vulkan_memory_allocator
  )
  list(APPEND INC_SYS
    ${VULKAN_INCLUDE_DIRS}
  )

  list(APPEND INC_SYS
    ${SHADERC_INCLUDE_DIRS}
  )
  list(APPEND SRC
    ${VULKAN_SRC}
  )

  list(APPEND LIB
    ${VULKAN_LIBRARIES}
    ${SHADERC_LIBRARIES}
    extern_vulkan_memory_allocator
    PRIVATE bf::extern::xxhash
  )

  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

set(GLSL_SRC
  GPU_shader_shared.hh
  GPU_xr_defines.hh

  shaders/infos/gpu_clip_planes_infos.hh
  shaders/infos/gpu_index_load_infos.hh
  shaders/infos/gpu_interface_infos.hh
  shaders/infos/gpu_shader_2D_area_borders_infos.hh
  shaders/infos/gpu_shader_2D_checker_infos.hh
  shaders/infos/gpu_shader_2D_diag_stripes_infos.hh
  shaders/infos/gpu_shader_2D_image_desaturate_color_infos.hh
  shaders/infos/gpu_shader_2D_image_infos.hh
  shaders/infos/gpu_shader_2D_image_overlays_merge_infos.hh
  shaders/infos/gpu_shader_2D_image_overlays_stereo_merge_infos.hh
  shaders/infos/gpu_shader_2D_image_rect_color_infos.hh
  shaders/infos/gpu_shader_2D_image_shuffle_color_infos.hh
  shaders/infos/gpu_shader_2D_node_socket_infos.hh
  shaders/infos/gpu_shader_2D_nodelink_infos.hh
  shaders/infos/gpu_shader_2D_point_uniform_size_uniform_color_aa_infos.hh
  shaders/infos/gpu_shader_2D_point_uniform_size_uniform_color_outline_aa_infos.hh
  shaders/infos/gpu_shader_2D_point_varying_size_varying_color_infos.hh
  shaders/infos/gpu_shader_2D_widget_infos.hh
  shaders/infos/gpu_shader_3D_depth_only_infos.hh
  shaders/infos/gpu_shader_3D_flat_color_infos.hh
  shaders/infos/gpu_shader_3D_image_infos.hh
  shaders/infos/gpu_shader_3D_point_infos.hh
  shaders/infos/gpu_shader_3D_polyline_infos.hh
  shaders/infos/gpu_shader_3D_smooth_color_infos.hh
  shaders/infos/gpu_shader_3D_uniform_color_infos.hh
  shaders/infos/gpu_shader_fullscreen_infos.hh
  shaders/infos/gpu_shader_gpencil_stroke_infos.hh
  shaders/infos/gpu_shader_icon_infos.hh
  shaders/infos/gpu_shader_index_infos.hh
  shaders/infos/gpu_shader_keyframe_shape_infos.hh
  shaders/infos/gpu_shader_line_dashed_uniform_color_infos.hh
  shaders/infos/gpu_shader_print_infos.hh
  shaders/infos/gpu_shader_sequencer_infos.hh
  shaders/infos/gpu_shader_simple_lighting_infos.hh
  shaders/infos/gpu_shader_text_infos.hh
  shaders/infos/gpu_shader_xr_raycast_infos.hh
  shaders/infos/gpu_srgb_to_framebuffer_space_infos.hh

  shaders/gpu_shader_depth_only_frag.glsl
  shaders/gpu_shader_uniform_color_frag.glsl
  shaders/gpu_shader_checker_frag.glsl
  shaders/gpu_shader_diag_stripes_frag.glsl
  shaders/gpu_shader_simple_lighting_frag.glsl
  shaders/gpu_shader_flat_color_frag.glsl
  shaders/gpu_shader_2D_vert.glsl
  shaders/gpu_shader_2D_area_borders_vert.glsl
  shaders/gpu_shader_2D_area_borders_frag.glsl
  shaders/gpu_shader_2D_widget_base_vert.glsl
  shaders/gpu_shader_2D_widget_base_frag.glsl
  shaders/gpu_shader_2D_widget_shadow_vert.glsl
  shaders/gpu_shader_2D_widget_shadow_frag.glsl
  shaders/gpu_shader_2D_node_socket_frag.glsl
  shaders/gpu_shader_2D_node_socket_vert.glsl
  shaders/gpu_shader_2D_nodelink_frag.glsl
  shaders/gpu_shader_2D_nodelink_vert.glsl
  shaders/gpu_shader_2D_line_dashed_frag.glsl
  shaders/gpu_shader_2D_image_vert.glsl
  shaders/gpu_shader_2D_image_rect_vert.glsl
  shaders/gpu_shader_icon_multi_vert.glsl
  shaders/gpu_shader_icon_frag.glsl
  shaders/gpu_shader_icon_vert.glsl
  shaders/gpu_shader_image_frag.glsl
  shaders/gpu_shader_image_desaturate_frag.glsl
  shaders/gpu_shader_image_overlays_merge_frag.glsl
  shaders/gpu_shader_image_overlays_stereo_merge_frag.glsl
  shaders/gpu_shader_image_shuffle_color_frag.glsl
  shaders/gpu_shader_image_color_frag.glsl
  shaders/gpu_shader_3D_image_vert.glsl
  shaders/gpu_shader_3D_vert.glsl
  shaders/gpu_shader_3D_normal_vert.glsl
  shaders/gpu_shader_3D_flat_color_vert.glsl
  shaders/gpu_shader_3D_line_dashed_uniform_color_vert.glsl
  shaders/gpu_shader_3D_polyline_frag.glsl
  shaders/gpu_shader_3D_polyline_vert.glsl
  shaders/gpu_shader_3D_smooth_color_vert.glsl
  shaders/gpu_shader_3D_smooth_color_frag.glsl
  shaders/gpu_shader_3D_clipped_uniform_color_vert.glsl

  shaders/gpu_shader_point_uniform_color_aa_frag.glsl
  shaders/gpu_shader_point_uniform_color_outline_aa_frag.glsl
  shaders/gpu_shader_point_varying_color_frag.glsl
  shaders/gpu_shader_3D_point_varying_size_varying_color_vert.glsl
  shaders/gpu_shader_3D_point_uniform_size_aa_vert.glsl
  shaders/gpu_shader_3D_point_flat_color_vert.glsl
  shaders/gpu_shader_2D_point_varying_size_varying_color_vert.glsl
  shaders/gpu_shader_2D_point_uniform_size_aa_vert.glsl
  shaders/gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl

  shaders/gpu_shader_text_vert.glsl
  shaders/gpu_shader_text_frag.glsl
  shaders/gpu_shader_xr_raycast_vert.glsl
  shaders/gpu_shader_keyframe_shape_vert.glsl
  shaders/gpu_shader_keyframe_shape_frag.glsl

  shaders/gpu_shader_sequencer_scope_comp.glsl
  shaders/gpu_shader_sequencer_scope_frag.glsl
  shaders/gpu_shader_sequencer_strips_vert.glsl
  shaders/gpu_shader_sequencer_strips_frag.glsl
  shaders/gpu_shader_sequencer_thumbs_vert.glsl
  shaders/gpu_shader_sequencer_thumbs_frag.glsl
  shaders/gpu_shader_sequencer_zebra_frag.glsl

  shaders/gpu_shader_codegen_lib.glsl

  shaders/common/gpu_shader_attribute_load_lib.glsl
  shaders/common/gpu_shader_bicubic_sampler_lib.glsl
  shaders/common/gpu_shader_common_color_ramp.glsl
  shaders/common/gpu_shader_common_color_utils.glsl
  shaders/common/gpu_shader_common_curves.glsl
  shaders/common/gpu_shader_common_hash.glsl
  shaders/common/gpu_shader_common_math_utils.glsl
  shaders/common/gpu_shader_common_math.glsl
  shaders/common/gpu_shader_common_mix_rgb.glsl
  shaders/common/gpu_shader_debug_gradients_lib.glsl
  shaders/common/gpu_shader_fullscreen_vert.glsl
  shaders/common/gpu_shader_index_load_lib.glsl
  shaders/common/gpu_shader_index_range_lib.glsl
  shaders/common/gpu_shader_math_angle_lib.glsl
  shaders/common/gpu_shader_math_axis_angle_lib.glsl
  shaders/common/gpu_shader_math_base_lib.glsl
  shaders/common/gpu_shader_math_constants_lib.glsl
  shaders/common/gpu_shader_math_euler_lib.glsl
  shaders/common/gpu_shader_math_fast_lib.glsl
  shaders/common/gpu_shader_math_matrix_adjoint_lib.glsl
  shaders/common/gpu_shader_math_matrix_compare_lib.glsl
  shaders/common/gpu_shader_math_matrix_construct_lib.glsl
  shaders/common/gpu_shader_math_matrix_conversion_lib.glsl
  shaders/common/gpu_shader_math_matrix_interpolate_lib.glsl
  shaders/common/gpu_shader_math_matrix_lib.glsl
  shaders/common/gpu_shader_math_matrix_normalize_lib.glsl
  shaders/common/gpu_shader_math_matrix_projection_lib.glsl
  shaders/common/gpu_shader_math_matrix_transform_lib.glsl
  shaders/common/gpu_shader_math_quaternion_lib.glsl
  shaders/common/gpu_shader_math_rotation_conversion_lib.glsl
  shaders/common/gpu_shader_math_rotation_lib.glsl
  shaders/common/gpu_shader_math_safe_lib.glsl
  shaders/common/gpu_shader_math_vector_compare_lib.glsl
  shaders/common/gpu_shader_math_vector_lib.glsl
  shaders/common/gpu_shader_math_vector_reduce_lib.glsl
  shaders/common/gpu_shader_math_vector_safe_lib.glsl
  shaders/common/gpu_shader_offset_indices_lib.glsl
  shaders/common/gpu_shader_print_lib.glsl
  shaders/common/gpu_shader_ray_utils_lib.glsl
  shaders/common/gpu_shader_ray_lib.glsl
  shaders/common/gpu_shader_sequencer_lib.glsl
  shaders/common/gpu_shader_shared_exponent_lib.glsl
  shaders/common/gpu_shader_smaa_lib.glsl
  shaders/common/gpu_shader_test_lib.glsl
  shaders/common/gpu_shader_utildefines_lib.glsl

  shaders/material/gpu_shader_material_add_shader.glsl
  shaders/material/gpu_shader_material_ambient_occlusion.glsl
  shaders/material/gpu_shader_material_attribute.glsl
  shaders/material/gpu_shader_material_background.glsl
  shaders/material/gpu_shader_material_bevel.glsl
  shaders/material/gpu_shader_material_wavelength.glsl
  shaders/material/gpu_shader_material_blackbody.glsl
  shaders/material/gpu_shader_material_bright_contrast.glsl
  shaders/material/gpu_shader_material_bump.glsl
  shaders/material/gpu_shader_material_camera.glsl
  shaders/material/gpu_shader_material_clamp.glsl
  shaders/material/gpu_shader_material_combine_color.glsl
  shaders/material/gpu_shader_material_combine_xyz.glsl
  shaders/material/gpu_shader_material_diffuse.glsl
  shaders/material/gpu_shader_material_displacement.glsl
  shaders/material/gpu_shader_material_eevee_specular.glsl
  shaders/material/gpu_shader_material_emission.glsl
  shaders/material/gpu_shader_material_fractal_noise.glsl
  shaders/material/gpu_shader_material_fractal_voronoi.glsl
  shaders/material/gpu_shader_material_fresnel.glsl
  shaders/material/gpu_shader_material_gamma.glsl
  shaders/material/gpu_shader_material_geometry.glsl
  shaders/material/gpu_shader_material_glass.glsl
  shaders/material/gpu_shader_material_glossy.glsl
  shaders/material/gpu_shader_material_hair_info.glsl
  shaders/material/gpu_shader_material_hair.glsl
  shaders/material/gpu_shader_material_holdout.glsl
  shaders/material/gpu_shader_material_hue_sat_val.glsl
  shaders/material/gpu_shader_material_invert.glsl
  shaders/material/gpu_shader_material_layer_weight.glsl
  shaders/material/gpu_shader_material_light_falloff.glsl
  shaders/material/gpu_shader_material_light_path.glsl
  shaders/material/gpu_shader_material_mapping.glsl
  shaders/material/gpu_shader_material_map_range.glsl
  shaders/material/gpu_shader_material_metallic.glsl
  shaders/material/gpu_shader_material_mix_color.glsl
  shaders/material/gpu_shader_material_mix_shader.glsl
  shaders/material/gpu_shader_material_noise.glsl
  shaders/material/gpu_shader_material_normal.glsl
  shaders/material/gpu_shader_material_normal_map.glsl
  shaders/material/gpu_shader_material_object_info.glsl
  shaders/material/gpu_shader_material_output_aov.glsl
  shaders/material/gpu_shader_material_output_material.glsl
  shaders/material/gpu_shader_material_output_world.glsl
  shaders/material/gpu_shader_material_particle_info.glsl
  shaders/material/gpu_shader_material_point_info.glsl
  shaders/material/gpu_shader_material_principled.glsl
  shaders/material/gpu_shader_material_ray_portal.glsl
  shaders/material/gpu_shader_material_refraction.glsl
  shaders/material/gpu_shader_material_repeat_zone.glsl
  shaders/material/gpu_shader_material_rgb_to_bw.glsl
  shaders/material/gpu_shader_material_radial_tiling.glsl
  shaders/material/gpu_shader_material_radial_tiling_shared.glsl
  shaders/material/gpu_shader_material_separate_color.glsl
  shaders/material/gpu_shader_material_separate_xyz.glsl
  shaders/material/gpu_shader_material_set.glsl
  shaders/material/gpu_shader_material_shader_to_rgba.glsl
  shaders/material/gpu_shader_material_sheen.glsl
  shaders/material/gpu_shader_material_squeeze.glsl
  shaders/material/gpu_shader_material_subsurface_scattering.glsl
  shaders/material/gpu_shader_material_tangent.glsl
  shaders/material/gpu_shader_material_tex_brick.glsl
  shaders/material/gpu_shader_material_tex_checker.glsl
  shaders/material/gpu_shader_material_tex_environment.glsl
  shaders/material/gpu_shader_material_tex_gabor.glsl
  shaders/material/gpu_shader_material_tex_gradient.glsl
  shaders/material/gpu_shader_material_tex_image.glsl
  shaders/material/gpu_shader_material_tex_magic.glsl
  shaders/material/gpu_shader_material_tex_noise.glsl
  shaders/material/gpu_shader_material_tex_sky.glsl
  shaders/material/gpu_shader_material_texture_coordinates.glsl
  shaders/material/gpu_shader_material_tex_voronoi.glsl
  shaders/material/gpu_shader_material_tex_wave.glsl
  shaders/material/gpu_shader_material_tex_white_noise.glsl
  shaders/material/gpu_shader_material_toon.glsl
  shaders/material/gpu_shader_material_transform_utils.glsl
  shaders/material/gpu_shader_material_translucent.glsl
  shaders/material/gpu_shader_material_transparent.glsl
  shaders/material/gpu_shader_material_uv_map.glsl
  shaders/material/gpu_shader_material_vector_displacement.glsl
  shaders/material/gpu_shader_material_vector_math.glsl
  shaders/material/gpu_shader_material_vector_rotate.glsl
  shaders/material/gpu_shader_material_vertex_color.glsl
  shaders/material/gpu_shader_material_volume_absorption.glsl
  shaders/material/gpu_shader_material_volume_principled.glsl
  shaders/material/gpu_shader_material_volume_scatter.glsl
  shaders/material/gpu_shader_material_volume_coefficients.glsl
  shaders/material/gpu_shader_material_voronoi.glsl
  shaders/material/gpu_shader_material_wireframe.glsl
  shaders/material/gpu_shader_material_world_normals.glsl

  shaders/gpu_shader_gpencil_stroke_vert.glsl
  shaders/gpu_shader_gpencil_stroke_frag.glsl

  shaders/gpu_shader_display_fallback_vert.glsl
  shaders/gpu_shader_display_fallback_frag.glsl

  shaders/gpu_shader_cfg_world_clip_lib.glsl
  shaders/gpu_shader_colorspace_lib.glsl

  shaders/gpu_shader_index_2d_array_points.glsl
  shaders/gpu_shader_index_2d_array_lines.glsl
  shaders/gpu_shader_index_2d_array_tris.glsl

  shaders/gpu_shader_compat_glsl.glsl

  shaders/gpu_shader_glsl_extension.glsl

  GPU_shader_shared_utils.hh
)

set(GLSL_SRC_TEST
  shaders/infos/gpu_shader_test_infos.hh

  tests/shaders/gpu_math_test.glsl
  tests/shaders/gpu_buffer_texture_test.glsl
  tests/shaders/gpu_compute_1d_test.glsl
  tests/shaders/gpu_compute_2d_test.glsl
  tests/shaders/gpu_compute_ibo_test.glsl
  tests/shaders/gpu_compute_ssbo_test.glsl
  tests/shaders/gpu_compute_vbo_test.glsl
  tests/shaders/gpu_compute_dummy_test.glsl
  tests/shaders/gpu_specialization_test.glsl
  tests/shaders/gpu_framebuffer_layer_viewport_test.glsl
  tests/shaders/gpu_framebuffer_subpass_input_test.glsl
  tests/shaders/gpu_push_constants_test.glsl
  tests/shaders/gpu_texture_test.glsl
  tests/shaders/gpu_texture_atomic_test.glsl
)

set(MTL_BACKEND_GLSL_SRC
  metal/kernels/depth_2d_update_infos.hh
  metal/kernels/gpu_shader_fullscreen_blit_infos.hh

  metal/kernels/depth_2d_update_float_frag.glsl
  metal/kernels/depth_2d_update_int24_frag.glsl
  metal/kernels/depth_2d_update_int32_frag.glsl
  metal/kernels/depth_2d_update_vert.glsl
  metal/kernels/gpu_shader_fullscreen_blit_vert.glsl
  metal/kernels/gpu_shader_fullscreen_blit_frag.glsl

  shaders/gpu_shader_msl_atomic.msl
  shaders/gpu_shader_msl_attribute.msl
  shaders/gpu_shader_msl_builtin.msl
  shaders/gpu_shader_msl_image.msl
  shaders/gpu_shader_msl_matrix_legacy.msl
  shaders/gpu_shader_msl_matrix.msl
  shaders/gpu_shader_msl_sampler.msl
  shaders/gpu_shader_msl_types.msl
  shaders/gpu_shader_msl_types_legacy.msl
  shaders/gpu_shader_msl_defines.msl
  shaders/gpu_shader_msl_wrapper.msl
  shaders/gpu_shader_compat_msl.msl
)

set(MSL_SRC
  metal/mtl_shader_shared.hh

  metal/kernels/compute_texture_update.msl
  metal/kernels/compute_texture_read.msl
)

set(VULKAN_BACKEND_GLSL_SRC
  vulkan/shaders/vk_backbuffer_blit_infos.hh

  vulkan/shaders/vk_backbuffer_blit_comp.glsl
)

if(WITH_GTESTS)
  if(WITH_GPU_BACKEND_TESTS)
    list(APPEND GLSL_SRC ${GLSL_SRC_TEST})
  endif()
endif()

if(WITH_METAL_BACKEND)
  list(APPEND GLSL_SRC ${MTL_BACKEND_GLSL_SRC})

  set(MSL_C)
  foreach(MSL_FILE ${MSL_SRC})
    data_to_c_simple(${MSL_FILE} MSL_C)
  endforeach()
endif()

if(WITH_VULKAN_BACKEND)
  list(APPEND GLSL_SRC ${VULKAN_BACKEND_GLSL_SRC})
endif()

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  glsl_to_c(${GLSL_FILE} GLSL_C)
endforeach()

set(SHADER_C)
list(APPEND SHADER_C ${GLSL_C})
if(WITH_METAL_BACKEND)
  list(APPEND SHADER_C ${MSL_C})
endif()

blender_add_lib(bf_gpu_shaders "${SHADER_C}" "" "" "")
blender_set_target_unity_build(bf_gpu_shaders 10)

list(APPEND LIB
  bf_gpu_shaders
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

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_gpu_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
set(glsl_metadata_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_gpu_metadata_list.hh")
file(GENERATE OUTPUT ${glsl_metadata_list_file} CONTENT "${GLSL_METADATA_CONTENT}")
list(APPEND SRC ${glsl_metadata_list_file})
set(glsl_infos_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_gpu_infos_list.hh")
file(GENERATE OUTPUT ${glsl_infos_list_file} CONTENT "${GLSL_INFOS_CONTENT}")
list(APPEND SRC ${glsl_infos_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_GPU_BACKEND_TESTS)
  add_definitions(-DWITH_GPU_BACKEND_TESTS)
endif()

if(WITH_GTESTS)
  add_definitions(-DWITH_GTESTS)
endif()

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::gpu ALIAS bf_gpu)
target_link_libraries(bf_gpu PUBLIC
  bf_compositor_shaders
  bf_draw_shaders
  bf_gpu_shaders
  bf_imbuf_opencolorio_shaders
)

if(WITH_OPENGL_BACKEND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(bf_gpu PUBLIC rt)
endif()

# If `execinfo.h` exists on a *BSD system then also link in `libexecinfo`.
# Needed for `backtrace` / `backtrace_symbols` (GNU extensions)
# brought in by `blenlib/intern/system.cc`.
if(HAVE_EXECINFO_H AND CMAKE_SYSTEM_NAME MATCHES "FreeBSD|NetBSD|OpenBSD|DragonFly")
  target_link_libraries(bf_gpu PUBLIC execinfo)
endif()

if(WITH_OPENSUBDIV)
  target_link_libraries(bf_gpu PUBLIC bf_osd_shaders)
endif()

if(WITH_RENDERDOC)
  target_link_libraries(bf_gpu PUBLIC bf_intern_renderdoc_dynload)
endif()


if(CXX_WARN_NO_SUGGEST_OVERRIDE)
  target_compile_options(bf_gpu PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>)
endif()


if(WITH_GTESTS)
  set(TEST_SRC)
  set(TEST_INC)
  set(TEST_LIB
    bf_intern_ghost
    bf_imbuf
    bf_windowmanager
  )

  if(WITH_GPU_BACKEND_TESTS)
    list(APPEND TEST_SRC
      tests/buffer_texture_test.cc
      tests/compute_test.cc
      tests/framebuffer_test.cc
      tests/immediate_test.cc
      tests/index_buffer_test.cc
      tests/push_constants_test.cc
      tests/shader_create_info_test.cc
      tests/shader_preprocess_test.cc
      tests/shader_test.cc
      tests/specialization_constants_test.cc
      tests/state_blend_test.cc
      tests/storage_buffer_test.cc
      tests/texture_test.cc
      tests/vertex_buffer_test.cc
    )
  endif()

  if(WITH_VULKAN_BACKEND)
    list(APPEND TEST_SRC
      vulkan/tests/vk_data_conversion_test.cc
      vulkan/tests/vk_memory_layout_test.cc
      vulkan/render_graph/tests/vk_render_graph_test_compute.cc
      vulkan/render_graph/tests/vk_render_graph_test_present.cc
      vulkan/render_graph/tests/vk_render_graph_test_render.cc
      vulkan/render_graph/tests/vk_render_graph_test_scheduler.cc
      vulkan/render_graph/tests/vk_render_graph_test_transfer.cc

      vulkan/render_graph/tests/vk_render_graph_test_types.hh
    )
  endif()

  # Enable shader validation on build-bot for Metal
  if(WITH_METAL_BACKEND AND NOT WITH_GPU_DRAW_TESTS AND
     NOT (WITH_GTESTS AND WITH_GPU_BACKEND_TESTS)) # Avoid duplicate source file
    list(APPEND TEST_SRC
      tests/shader_create_info_test.cc
    )
  endif()

  set(TEST_COMMON_SRC
    tests/gpu_testing.cc
    tests/gpu_testing.hh
  )

  blender_add_test_suite_lib(gpu
    "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}" "${TEST_COMMON_SRC}"
  )
endif()
