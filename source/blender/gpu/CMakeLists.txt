# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  dummy
  intern
  metal
  opengl
  shader_tool
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
  shaders/compat
  shaders/python
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
  intern/gpu_ray_tracing.cc
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
  intern/gpu_shader_preprocessor.cc
  intern/gpu_state.cc
  intern/gpu_storage_buffer.cc
  intern/gpu_texture.cc
  intern/gpu_texture_mipmap.cc
  intern/gpu_texture_pool.cc
  intern/gpu_uniform_buffer.cc
  intern/gpu_vertex_buffer.cc
  intern/gpu_vertex_format.cc
  intern/gpu_vertex_format_normals.cc
  intern/gpu_viewport.cc
  intern/gpu_work_in_flight.cc
  intern/gpu_worker.cc

  shader_tool/ast.cc
  shader_tool/attribute.cc
  shader_tool/enum.cc
  shader_tool/flow_control.cc
  shader_tool/function.cc
  shader_tool/grammar.cc
  shader_tool/intermediate.cc
  shader_tool/metadata.cc
  shader_tool/namespace.cc
  shader_tool/processor.cc
  shader_tool/resource_table.cc
  shader_tool/string.cc
  shader_tool/struct.cc
  shader_tool/template.cc
  shader_tool/union.cc

  shader_tool/lexit/lexit.cc

  shader_tool/bsl/codegen.cc
  shader_tool/bsl/constructor.cc
  shader_tool/bsl/grammar.cc
  shader_tool/bsl/operator.cc
  shader_tool/bsl/resource.cc
  shader_tool/bsl/symbol.cc
  shader_tool/bsl/symbol_class.cc
  shader_tool/bsl/symbol_function.cc
  shader_tool/bsl/symbol_scope.cc
  shader_tool/bsl/symbol_table.cc
  shader_tool/bsl/symbol_template.cc
  shader_tool/bsl/symbol_variable.cc
  shader_tool/bsl/type_checker.cc

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
  GPU_ray_tracing.hh
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
  GPU_work_in_flight.hh
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
  intern/gpu_shader_create_info_pipeline.hh
  intern/gpu_shader_create_info_private.hh
  intern/gpu_shader_dead_code_elimination.hh
  intern/gpu_shader_dependency_private.hh
  intern/gpu_shader_interface.hh
  intern/gpu_shader_metadata_private.hh
  intern/gpu_shader_private.hh
  intern/gpu_state_private.hh
  intern/gpu_storage_buffer_private.hh
  intern/gpu_texture_pool_private.hh
  intern/gpu_texture_private.hh
  intern/gpu_uniform_buffer_private.hh
  intern/gpu_vertex_format_private.hh

  dummy/dummy_backend.hh
  dummy/dummy_batch.hh
  dummy/dummy_context.hh
  dummy/dummy_framebuffer.hh
  dummy/dummy_vertex_buffer.hh

  shaders/compat/gpu_shader_compat.hh
  shaders/compat/gpu_shader_compat_cxx.hh
  shaders/compat/gpu_shader_cxx_attribute.hh
  shaders/compat/gpu_shader_cxx_builtin.hh
  shaders/compat/gpu_shader_cxx_global.hh
  shaders/compat/gpu_shader_cxx_image.hh
  shaders/compat/gpu_shader_cxx_matrix.hh
  shaders/compat/gpu_shader_cxx_sampler.hh
  shaders/compat/gpu_shader_cxx_string.hh
  shaders/compat/gpu_shader_cxx_vector.hh
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
  opengl/gl_texture_pool.cc
  opengl/gl_uniform_buffer.cc
  opengl/gl_vertex_array.cc
  opengl/gl_vertex_buffer.cc
  opengl/gl_work_in_flight.cc

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
  opengl/gl_texture_pool.hh
  opengl/gl_uniform_buffer.hh
  opengl/gl_vertex_array.hh
  opengl/gl_vertex_buffer.hh
  opengl/gl_work_in_flight.hh
)

set(VULKAN_SRC
  vulkan/vk_backend.cc
  vulkan/vk_batch.cc
  vulkan/vk_buffer.cc
  vulkan/vk_buffer_pool.cc
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
  vulkan/vk_ray_tracing.cc
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
  vulkan/vk_texture_pool.cc
  vulkan/vk_to_string.cc
  vulkan/vk_uniform_buffer.cc
  vulkan/vk_vertex_attribute_object.cc
  vulkan/vk_vertex_attribute_object_cache.cc
  vulkan/vk_vertex_buffer.cc
  vulkan/vk_work_in_flight.cc

  vulkan/vk_backend.hh
  vulkan/vk_batch.hh
  vulkan/vk_buffer.hh
  vulkan/vk_buffer_pool.hh
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
  vulkan/vk_graphics_pipeline.hh
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
  vulkan/vk_ray_tracing.hh
  vulkan/render_graph/nodes/vk_begin_query_node.hh
  vulkan/render_graph/nodes/vk_begin_rendering_node.hh
  vulkan/render_graph/nodes/vk_blit_image_node.hh
  vulkan/render_graph/nodes/vk_build_acceleration_structure_node.hh
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
  vulkan/vk_texture_pool.hh
  vulkan/vk_to_string.hh
  vulkan/vk_uniform_buffer.hh
  vulkan/vk_vertex_attribute_object.hh
  vulkan/vk_vertex_attribute_object_cache.hh
  vulkan/vk_vertex_buffer.hh
  vulkan/vk_vertex_input_description.hh
  vulkan/vk_work_in_flight.hh
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
  metal/mtl_ray_tracing.mm
  metal/mtl_shader.mm
  metal/mtl_shader_generate.cc
  metal/mtl_shader_interface.mm
  metal/mtl_shader_log.mm
  metal/mtl_state.mm
  metal/mtl_storage_buffer.mm
  metal/mtl_texture.mm
  metal/mtl_texture_pool.mm
  metal/mtl_texture_util.mm
  metal/mtl_uniform_buffer.mm
  metal/mtl_vertex_buffer.mm
  metal/mtl_work_in_flight.mm

  metal/mtl_backend.hh
  metal/mtl_batch.hh
  metal/mtl_capabilities.hh
  metal/mtl_command_buffer.hh
  metal/mtl_common.hh
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
  metal/mtl_ray_tracing.hh
  metal/mtl_shader.hh
  metal/mtl_shader_generate.hh
  metal/mtl_shader_interface.hh
  metal/mtl_shader_interface_type.hh
  metal/mtl_shader_log.hh
  metal/mtl_shader_shared.hh
  metal/mtl_state.hh
  metal/mtl_storage_buffer.hh
  metal/mtl_texture.hh
  metal/mtl_texture_pool.hh
  metal/mtl_uniform_buffer.hh
  metal/mtl_vertex_buffer.hh
  metal/mtl_work_in_flight.hh
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
  PRIVATE bf::nodes
  PRIVATE bf::dependencies::opencolorio
)

# Select Backend source based on availability
if(WITH_OPENGL_BACKEND)
  list(APPEND SRC
    ${OPENGL_SRC}
  )
  list(APPEND LIB
    PRIVATE bf::dependencies::epoxy
  )
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_METAL_BACKEND)
  list(APPEND SRC ${METAL_SRC})
endif()


if(WITH_VULKAN_BACKEND)
  list(APPEND INC
    ../../../extern/volk
    ../../../extern/vulkan_memory_allocator
  )
  list(APPEND INC_SYS
    ${VULKAN_INCLUDE_DIRS}
    ${SHADERC_INCLUDE_DIRS}
  )
  list(APPEND SRC
    ${VULKAN_SRC}
  )
  list(APPEND LIB
    PRIVATE bf::extern::vulkan_memory_allocator
    PRIVATE bf::extern::xxhash
    PRIVATE bf::dependencies::optional::vulkan
    PRIVATE bf::dependencies::optional::shaderc
    ${SHADERC_LIBRARIES}
    extern_volk
    extern_vulkan_memory_allocator
  )

  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

set(GLSL_SRC
  GPU_shader_shared.hh

  shaders/infos/gpu_index_load_infos.hh

  shaders/infos/gpu_shader_fullscreen_infos.hh
  shaders/infos/gpu_shader_print_infos.hh
  shaders/infos/gpu_srgb_to_framebuffer_space_infos.hh

  shaders/gpu_shader_area_borders.bsl.hh

  shaders/gpu_shader_widget_base.bsl.hh
  shaders/gpu_shader_widget_shadow.bsl.hh
  shaders/gpu_shader_node_socket.bsl.hh
  shaders/gpu_shader_nodelink.bsl.hh
  shaders/gpu_shader_icon.bsl.hh
  shaders/gpu_shader_polyline.bsl.hh
  shaders/gpu_shader_update_mipmaps.bsl.hh

  shaders/gpu_shader_text.bsl.hh
  shaders/gpu_shader_keyframe_shape.bsl.hh

  shaders/gpu_shader_sequencer_scope.bsl.hh
  shaders/gpu_shader_sequencer_widget.bsl.hh
  shaders/gpu_shader_sequencer_zebra.bsl.hh

  shaders/gpu_shader_codegen_lib.glsl

  shaders/python/gpu_shader_python_colorspace_lib.glsl
  shaders/python/gpu_shader_python_comp.glsl
  shaders/python/gpu_shader_python_frag.glsl
  shaders/python/gpu_shader_python_typedef_lib.glsl
  shaders/python/gpu_shader_python_vert.glsl

  shaders/gpu_shader_image.bsl.hh
  shaders/gpu_shader_simple.bsl.hh
  shaders/gpu_shader_colorspace.bsl.hh

  shaders/common/gpu_shader_attribute_load_lib.glsl
  shaders/common/gpu_shader_bicubic_sampler.bsl.hh
  shaders/common/gpu_shader_debug_gradients.bsl.hh
  shaders/common/gpu_shader_fullscreen_vert.glsl
  shaders/common/gpu_shader_fullscreen.bsl.hh
  shaders/common/gpu_shader_index_load_lib.glsl
  shaders/common/gpu_shader_index_load.bsl.hh
  shaders/common/gpu_shader_index_range_lib.glsl
  shaders/common/gpu_shader_math_angle.bsl.hh
  shaders/common/gpu_shader_math_axis_angle.bsl.hh
  shaders/common/gpu_shader_math_base.bsl.hh
  shaders/common/gpu_shader_math_constants.bsl.hh
  shaders/common/gpu_shader_math_euler.bsl.hh
  shaders/common/gpu_shader_math_fast.bsl.hh
  shaders/common/gpu_shader_math_matrix_adjoint.bsl.hh
  shaders/common/gpu_shader_math_matrix_compare.bsl.hh
  shaders/common/gpu_shader_math_matrix_construct.bsl.hh
  shaders/common/gpu_shader_math_matrix_conversion.bsl.hh
  shaders/common/gpu_shader_math_matrix_interpolate.bsl.hh
  shaders/common/gpu_shader_math_matrix.bsl.hh
  shaders/common/gpu_shader_math_matrix_normalize.bsl.hh
  shaders/common/gpu_shader_math_matrix_projection.bsl.hh
  shaders/common/gpu_shader_math_matrix_transform.bsl.hh
  shaders/common/gpu_shader_math_quaternion.bsl.hh
  shaders/common/gpu_shader_math_rotation_conversion.bsl.hh
  shaders/common/gpu_shader_math_rotation.bsl.hh
  shaders/common/gpu_shader_math_safe.bsl.hh
  shaders/common/gpu_shader_math_vector_compare.bsl.hh
  shaders/common/gpu_shader_math_vector.bsl.hh
  shaders/common/gpu_shader_math_vector_reduce.bsl.hh
  shaders/common/gpu_shader_math_vector_safe.bsl.hh
  shaders/common/gpu_shader_offset_indices_lib.glsl
  shaders/common/gpu_shader_print_lib.glsl
  shaders/common/gpu_shader_ray_utils.bsl.hh
  shaders/common/gpu_shader_ray.bsl.hh
  shaders/common/gpu_shader_shared_exponent.bsl.hh
  shaders/common/gpu_shader_smaa_lib.glsl
  shaders/common/gpu_shader_test_lib.bsl.hh
  shaders/common/gpu_shader_tiled_image_lookup.bsl.hh
  shaders/common/gpu_shader_utildefines.bsl.hh

  shaders/nodes/gpu_shader_common_color_ramp.bsl.hh
  shaders/nodes/gpu_shader_common_color_utils.bsl.hh
  shaders/nodes/gpu_shader_common_curves.bsl.hh
  shaders/nodes/gpu_shader_common_hash.bsl.hh
  shaders/nodes/gpu_shader_common_math.bsl.hh
  shaders/nodes/gpu_shader_common_math_utils.bsl.hh
  shaders/nodes/gpu_shader_common_mix_rgb.bsl.hh

  shaders/material/gpu_shader_material_interface.bsl.hh

  shaders/material/gpu_shader_material_add_shader.bsl.hh
  shaders/material/gpu_shader_material_align_rotation_to_vector.bsl.hh
  shaders/material/gpu_shader_material_ambient_occlusion.bsl.hh
  shaders/material/gpu_shader_material_attribute.bsl.hh
  shaders/material/gpu_shader_material_axes_to_rotation.bsl.hh
  shaders/material/gpu_shader_material_axis_angle_to_rotation.bsl.hh
  shaders/material/gpu_shader_material_background.bsl.hh
  shaders/material/gpu_shader_material_bevel.bsl.hh
  shaders/material/gpu_shader_material_bit_math.bsl.hh
  shaders/material/gpu_shader_material_blackbody.bsl.hh
  shaders/material/gpu_shader_material_boolean_math.bsl.hh
  shaders/material/gpu_shader_material_bright_contrast.bsl.hh
  shaders/material/gpu_shader_material_bump.glsl
  shaders/material/gpu_shader_material_camera.glsl
  shaders/material/gpu_shader_material_clamp.bsl.hh
  shaders/material/gpu_shader_material_combine_color.bsl.hh
  shaders/material/gpu_shader_material_combine_transform.bsl.hh
  shaders/material/gpu_shader_material_combine_xyz.bsl.hh
  shaders/material/gpu_shader_material_compare.bsl.hh
  shaders/material/gpu_shader_material_diffuse.bsl.hh
  shaders/material/gpu_shader_material_displacement.glsl
  shaders/material/gpu_shader_material_eevee_specular.glsl
  shaders/material/gpu_shader_material_emission.bsl.hh
  shaders/material/gpu_shader_material_euler_to_rotation.bsl.hh
  shaders/material/gpu_shader_material_float_to_int.bsl.hh
  shaders/material/gpu_shader_material_fractal_noise.bsl.hh
  shaders/material/gpu_shader_material_fractal_voronoi.bsl.hh
  shaders/material/gpu_shader_material_fresnel.bsl.hh
  shaders/material/gpu_shader_material_gamma.bsl.hh
  shaders/material/gpu_shader_material_geometry.bsl.hh
  shaders/material/gpu_shader_material_get_vector_component.bsl.hh
  shaders/material/gpu_shader_material_glass.bsl.hh
  shaders/material/gpu_shader_material_glossy.glsl
  shaders/material/gpu_shader_material_hair.bsl.hh
  shaders/material/gpu_shader_material_hair_info.bsl.hh
  shaders/material/gpu_shader_material_hash_value.bsl.hh
  shaders/material/gpu_shader_material_holdout.bsl.hh
  shaders/material/gpu_shader_material_hue_sat_val.bsl.hh
  shaders/material/gpu_shader_material_implicit_defaults.bsl.hh
  shaders/material/gpu_shader_material_integer_math.bsl.hh
  shaders/material/gpu_shader_material_invert.bsl.hh
  shaders/material/gpu_shader_material_invert_rotation.bsl.hh
  shaders/material/gpu_shader_material_layer_weight.bsl.hh
  shaders/material/gpu_shader_material_light_accumulation.bsl.hh
  shaders/material/gpu_shader_material_light_evaluation.bsl.hh
  shaders/material/gpu_shader_material_light_falloff.bsl.hh
  shaders/material/gpu_shader_material_light_info.bsl.hh
  shaders/material/gpu_shader_material_light_iter_internal.bsl.hh
  shaders/material/gpu_shader_material_light_path.glsl
  shaders/material/gpu_shader_material_map_range.bsl.hh
  shaders/material/gpu_shader_material_mapping.bsl.hh
  shaders/material/gpu_shader_material_matrix_svd.bsl.hh
  shaders/material/gpu_shader_material_metallic.bsl.hh
  shaders/material/gpu_shader_material_mix_color.bsl.hh
  shaders/material/gpu_shader_material_mix_shader.bsl.hh
  shaders/material/gpu_shader_material_noise.bsl.hh
  shaders/material/gpu_shader_material_normal.bsl.hh
  shaders/material/gpu_shader_material_normal_map.glsl
  shaders/material/gpu_shader_material_object_info.glsl
  shaders/material/gpu_shader_material_open_pbr_util.bsl.hh
  shaders/material/gpu_shader_material_output_aov.glsl
  shaders/material/gpu_shader_material_output_material.glsl
  shaders/material/gpu_shader_material_output_world.bsl.hh
  shaders/material/gpu_shader_material_particle_info.bsl.hh
  shaders/material/gpu_shader_material_point_info.bsl.hh
  shaders/material/gpu_shader_material_principled.bsl.hh
  shaders/material/gpu_shader_material_quaternion_to_rotation.bsl.hh
  shaders/material/gpu_shader_material_radial_tiling.bsl.hh
  shaders/material/gpu_shader_material_radial_tiling_shared.glsl
  shaders/material/gpu_shader_material_random_value.bsl.hh
  shaders/material/gpu_shader_material_ray_portal.bsl.hh
  shaders/material/gpu_shader_material_raycast.bsl.hh
  shaders/material/gpu_shader_material_refraction.bsl.hh
  shaders/material/gpu_shader_material_repeat_zone.bsl.hh
  shaders/material/gpu_shader_material_rgb_to_bw.bsl.hh
  shaders/material/gpu_shader_material_rotate_rotation.bsl.hh
  shaders/material/gpu_shader_material_rotate_vector.bsl.hh
  shaders/material/gpu_shader_material_rotation_to_axis_angle.bsl.hh
  shaders/material/gpu_shader_material_rotation_to_euler.bsl.hh
  shaders/material/gpu_shader_material_rotation_to_quaternion.bsl.hh
  shaders/material/gpu_shader_material_scene_time.bsl.hh
  shaders/material/gpu_shader_material_separate_color.bsl.hh
  shaders/material/gpu_shader_material_separate_transform.bsl.hh
  shaders/material/gpu_shader_material_separate_xyz.bsl.hh
  shaders/material/gpu_shader_material_set.bsl.hh
  shaders/material/gpu_shader_material_shader_to_rgba.bsl.hh
  shaders/material/gpu_shader_material_shadow_raycast.bsl.hh
  shaders/material/gpu_shader_material_sheen.bsl.hh
  shaders/material/gpu_shader_material_squeeze.bsl.hh
  shaders/material/gpu_shader_material_subsurface_scattering.bsl.hh
  shaders/material/gpu_shader_material_tangent.glsl
  shaders/material/gpu_shader_material_tex_brick.bsl.hh
  shaders/material/gpu_shader_material_tex_checker.bsl.hh
  shaders/material/gpu_shader_material_tex_environment.bsl.hh
  shaders/material/gpu_shader_material_tex_gabor.bsl.hh
  shaders/material/gpu_shader_material_tex_gradient.bsl.hh
  shaders/material/gpu_shader_material_tex_image.bsl.hh
  shaders/material/gpu_shader_material_tex_magic.bsl.hh
  shaders/material/gpu_shader_material_tex_noise.bsl.hh
  shaders/material/gpu_shader_material_tex_sky.bsl.hh
  shaders/material/gpu_shader_material_tex_voronoi.bsl.hh
  shaders/material/gpu_shader_material_tex_wave.bsl.hh
  shaders/material/gpu_shader_material_tex_white_noise.bsl.hh
  shaders/material/gpu_shader_material_texture_coordinates.glsl
  shaders/material/gpu_shader_material_toon.bsl.hh
  shaders/material/gpu_shader_material_transform_utils.glsl
  shaders/material/gpu_shader_material_translucent.bsl.hh
  shaders/material/gpu_shader_material_transparent.bsl.hh
  shaders/material/gpu_shader_material_uv_map.bsl.hh
  shaders/material/gpu_shader_material_vector_displacement.glsl
  shaders/material/gpu_shader_material_vector_math.bsl.hh
  shaders/material/gpu_shader_material_vector_rotate.bsl.hh
  shaders/material/gpu_shader_material_vertex_color.bsl.hh
  shaders/material/gpu_shader_material_volume_absorption.bsl.hh
  shaders/material/gpu_shader_material_volume_coefficients.bsl.hh
  shaders/material/gpu_shader_material_volume_principled.bsl.hh
  shaders/material/gpu_shader_material_volume_scatter.bsl.hh
  shaders/material/gpu_shader_material_voronoi.bsl.hh
  shaders/material/gpu_shader_material_wavelength.bsl.hh
  shaders/material/gpu_shader_material_wireframe.bsl.hh

  shaders/gpu_shader_annotation.bsl.hh

  shaders/gpu_shader_display_fallback.bsl.hh

  shaders/gpu_shader_index_gen.bsl.hh

  shaders/compat/gpu_shader_compat_glsl.glsl
  shaders/compat/gpu_shader_glsl_extension.glsl

  GPU_shader_shared_utils.hh
)

set(GLSL_TEST_SRC
  shaders/infos/gpu_shader_test_infos.hh

  tests/shaders/gpu_math_test.bsl.hh
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
  tests/shaders/gpu_ray_query_test.glsl
  tests/shaders/gpu_ray_query_raster_test.glsl
  tests/shaders/gpu_ray_query_custom_index_test.glsl
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

  shaders/compat/gpu_shader_msl_atomic.msl
  shaders/compat/gpu_shader_msl_attribute.msl
  shaders/compat/gpu_shader_msl_builtin.msl
  shaders/compat/gpu_shader_msl_image.msl
  shaders/compat/gpu_shader_msl_matrix_legacy.msl
  shaders/compat/gpu_shader_msl_matrix.msl
  shaders/compat/gpu_shader_msl_ray_query.msl
  shaders/compat/gpu_shader_msl_sampler.msl
  shaders/compat/gpu_shader_msl_string.msl
  shaders/compat/gpu_shader_msl_types.msl
  shaders/compat/gpu_shader_msl_types_legacy.msl
  shaders/compat/gpu_shader_msl_defines.msl
  shaders/compat/gpu_shader_msl_wrapper.msl
  shaders/compat/gpu_shader_compat_msl.msl
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
    list(APPEND GLSL_SRC ${GLSL_TEST_SRC})
  endif()
endif()

if(WITH_METAL_BACKEND)
  list(APPEND GLSL_SRC ${MTL_BACKEND_GLSL_SRC})

  set(MSL_C "")
  foreach(MSL_FILE ${MSL_SRC})
    data_to_c_simple(${MSL_FILE} MSL_C)
  endforeach()
endif()

if(WITH_VULKAN_BACKEND)
  list(APPEND GLSL_SRC ${VULKAN_BACKEND_GLSL_SRC})
endif()

set(GLSL_INC
  .

  intern
  shaders
  shaders/common
  shaders/compat
  shaders/infos
  shaders/nodes
  shaders/material
  shaders/python

  vulkan/shaders
)

set(GLSL_C "")
foreach(GLSL_FILE ${GLSL_SRC})
  glsl_to_c(${GLSL_FILE} GLSL_C GLSL_INC)
endforeach()

set(SHADER_C "")
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
  cmake_path(GET GLSL_FILE FILENAME GLSL_FILE_NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_FILE_NAME_UNDERSCORES "_tmp")
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
  set(TEST_SRC "")
  set(TEST_INC "")
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
      tests/mipmap_test.cc
      tests/push_constants_test.cc
      tests/ray_tracing_test.cc
      tests/shader_create_info_test.cc
      tests/shader_test.cc
      tests/specialization_constants_test.cc
      tests/state_blend_test.cc
      tests/storage_buffer_test.cc
      tests/texture_test.cc
      tests/texture_view_test.cc
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

  set(TEST_COMMON_SRC
    tests/gpu_testing.cc
    tests/gpu_testing.hh
  )

  blender_add_test_suite_lib(gpu
    "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}" "${TEST_COMMON_SRC}"
  )
endif()
