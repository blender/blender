# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

# WITH_OPENGL limits the visibility of the opengl headers to just GPU and bg_gpu,
# to more easily highlight code-paths in other libraries that need to be refactored,
# bf_gpu is allowed to have opengl regardless of this option.

if(NOT WITH_OPENGL AND NOT WITH_METAL_BACKEND)
  add_definitions(-DWITH_OPENGL)
endif()

set(INC
  .
  intern
  metal
  opengl
  ../blenkernel
  ../blenlib
  ../bmesh
  ../draw
  ../imbuf
  ../makesdna
  ../makesrna
  ../windowmanager

  ../editors/include

  # For node muting stuff...
  ../nodes
  ../nodes/intern

  ../../../intern/atomic
  ../../../intern/clog
  ../../../intern/ghost
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/mantaflow/extern
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  intern/gpu_batch.cc
  intern/gpu_batch_presets.c
  intern/gpu_batch_utils.c
  intern/gpu_buffers.c
  intern/gpu_capabilities.cc
  intern/gpu_codegen.cc
  intern/gpu_compute.cc
  intern/gpu_context.cc
  intern/gpu_debug.cc
  intern/gpu_drawlist.cc
  intern/gpu_framebuffer.cc
  intern/gpu_immediate.cc
  intern/gpu_immediate_util.c
  intern/gpu_index_buffer.cc
  intern/gpu_init_exit.c
  intern/gpu_material.c
  intern/gpu_matrix.cc
  intern/gpu_node_graph.c
  intern/gpu_platform.cc
  intern/gpu_query.cc
  intern/gpu_select.c
  intern/gpu_select_pick.c
  intern/gpu_select_sample_query.cc
  intern/gpu_shader.cc
  intern/gpu_shader_builtin.c
  intern/gpu_shader_create_info.cc
  intern/gpu_shader_dependency.cc
  intern/gpu_shader_interface.cc
  intern/gpu_shader_log.cc
  intern/gpu_state.cc
  intern/gpu_storage_buffer.cc
  intern/gpu_texture.cc
  intern/gpu_uniform_buffer.cc
  intern/gpu_vertex_buffer.cc
  intern/gpu_vertex_format.cc
  intern/gpu_viewport.c

  GPU_batch.h
  GPU_batch_presets.h
  GPU_batch_utils.h
  GPU_buffers.h
  GPU_capabilities.h
  GPU_common.h
  GPU_common_types.h
  GPU_compute.h
  GPU_context.h
  GPU_debug.h
  GPU_drawlist.h
  GPU_framebuffer.h
  GPU_glew.h
  GPU_immediate.h
  GPU_immediate_util.h
  GPU_index_buffer.h
  GPU_init_exit.h
  GPU_legacy_stubs.h
  GPU_material.h
  GPU_matrix.h
  GPU_platform.h
  GPU_primitive.h
  GPU_select.h
  GPU_shader.h
  GPU_shader_shared.h
  GPU_state.h
  GPU_storage_buffer.h
  GPU_texture.h
  GPU_uniform_buffer.h
  GPU_vertex_buffer.h
  GPU_vertex_format.h
  GPU_viewport.h

  intern/gpu_backend.hh
  intern/gpu_batch_private.hh
  intern/gpu_capabilities_private.hh
  intern/gpu_codegen.h
  intern/gpu_context_private.hh
  intern/gpu_debug_private.hh
  intern/gpu_drawlist_private.hh
  intern/gpu_framebuffer_private.hh
  intern/gpu_immediate_private.hh
  intern/gpu_index_buffer_private.hh
  intern/gpu_material_library.h
  intern/gpu_matrix_private.h
  intern/gpu_node_graph.h
  intern/gpu_platform_private.hh
  intern/gpu_private.h
  intern/gpu_query.hh
  intern/gpu_select_private.h
  intern/gpu_shader_create_info.hh
  intern/gpu_shader_create_info_private.hh
  intern/gpu_shader_dependency_private.h
  intern/gpu_shader_interface.hh
  intern/gpu_shader_private.hh
  intern/gpu_state_private.hh
  intern/gpu_storage_buffer_private.hh
  intern/gpu_texture_private.hh
  intern/gpu_uniform_buffer_private.hh
  intern/gpu_vertex_buffer_private.hh
  intern/gpu_vertex_format_private.h
)

set(OPENGL_SRC

  opengl/gl_backend.cc
  opengl/gl_batch.cc
  opengl/gl_compute.cc
  opengl/gl_context.cc
  opengl/gl_debug.cc
  opengl/gl_debug_layer.cc
  opengl/gl_drawlist.cc
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
  opengl/gl_compute.hh
  opengl/gl_context.hh
  opengl/gl_debug.hh
  opengl/gl_drawlist.hh
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

set(METAL_SRC
  metal/mtl_backend.mm
  metal/mtl_context.mm
  metal/mtl_debug.mm
  metal/mtl_state.mm
  metal/mtl_texture.mm
  metal/mtl_texture_util.mm

  metal/mtl_backend.hh
  metal/mtl_capabilities.hh
  metal/mtl_common.hh
  metal/mtl_context.hh
  metal/mtl_debug.hh
  metal/mtl_state.hh
  metal/mtl_texture.hh
)

# Select Backend source based on availability
if(WITH_OPENGL)
  list(APPEND SRC ${OPENGL_SRC})
endif()

if(WITH_METAL_BACKEND)
  list(APPEND SRC ${METAL_SRC})
endif()

set(LIB
  ${BLENDER_GL_LIBRARIES}
)

if(NOT WITH_SYSTEM_GLEW)
  list(APPEND LIB
    ${BLENDER_GLEW_LIBRARIES}
  )
endif()

set(MSL_SRC

  metal/kernels/compute_texture_update.msl
  metal/kernels/compute_texture_read.msl
  metal/kernels/depth_2d_update_float_frag.glsl
  metal/kernels/depth_2d_update_int24_frag.glsl
  metal/kernels/depth_2d_update_int32_frag.glsl
  metal/kernels/depth_2d_update_vert.glsl
  metal/kernels/gpu_shader_fullscreen_blit_vert.glsl
  metal/kernels/gpu_shader_fullscreen_blit_frag.glsl
)

set(GLSL_SRC
  GPU_shader_shared.h
  shaders/opengl/glsl_shader_defines.glsl

  shaders/gpu_shader_depth_only_frag.glsl
  shaders/gpu_shader_uniform_color_frag.glsl
  shaders/gpu_shader_checker_frag.glsl
  shaders/gpu_shader_diag_stripes_frag.glsl
  shaders/gpu_shader_simple_lighting_frag.glsl
  shaders/gpu_shader_flat_color_frag.glsl
  shaders/gpu_shader_flat_color_alpha_test_0_frag.glsl
  shaders/gpu_shader_flat_id_frag.glsl
  shaders/gpu_shader_2D_vert.glsl
  shaders/gpu_shader_2D_area_borders_vert.glsl
  shaders/gpu_shader_2D_area_borders_frag.glsl
  shaders/gpu_shader_2D_widget_base_vert.glsl
  shaders/gpu_shader_2D_widget_base_frag.glsl
  shaders/gpu_shader_2D_widget_shadow_vert.glsl
  shaders/gpu_shader_2D_widget_shadow_frag.glsl
  shaders/gpu_shader_2D_nodelink_frag.glsl
  shaders/gpu_shader_2D_nodelink_vert.glsl
  shaders/gpu_shader_2D_flat_color_vert.glsl
  shaders/gpu_shader_2D_line_dashed_uniform_color_vert.glsl
  shaders/gpu_shader_2D_line_dashed_frag.glsl
  shaders/gpu_shader_2D_smooth_color_vert.glsl
  shaders/gpu_shader_2D_smooth_color_frag.glsl
  shaders/gpu_shader_2D_image_vert.glsl
  shaders/gpu_shader_2D_image_rect_vert.glsl
  shaders/gpu_shader_2D_image_multi_rect_vert.glsl
  shaders/gpu_shader_image_frag.glsl
  shaders/gpu_shader_image_desaturate_frag.glsl
  shaders/gpu_shader_image_overlays_merge_frag.glsl
  shaders/gpu_shader_image_overlays_stereo_merge_frag.glsl
  shaders/gpu_shader_image_modulate_alpha_frag.glsl
  shaders/gpu_shader_image_shuffle_color_frag.glsl
  shaders/gpu_shader_image_color_frag.glsl
  shaders/gpu_shader_image_varying_color_frag.glsl
  shaders/gpu_shader_3D_image_vert.glsl
  shaders/gpu_shader_3D_vert.glsl
  shaders/gpu_shader_3D_normal_vert.glsl
  shaders/gpu_shader_3D_flat_color_vert.glsl
  shaders/gpu_shader_3D_line_dashed_uniform_color_vert.glsl
  shaders/gpu_shader_3D_polyline_frag.glsl
  shaders/gpu_shader_3D_polyline_geom.glsl
  shaders/gpu_shader_3D_polyline_vert.glsl
  shaders/gpu_shader_3D_smooth_color_vert.glsl
  shaders/gpu_shader_3D_smooth_color_frag.glsl
  shaders/gpu_shader_3D_passthrough_vert.glsl
  shaders/gpu_shader_3D_clipped_uniform_color_vert.glsl

  shaders/gpu_shader_instance_variying_size_variying_color_vert.glsl

  shaders/gpu_shader_point_uniform_color_aa_frag.glsl
  shaders/gpu_shader_point_uniform_color_outline_aa_frag.glsl
  shaders/gpu_shader_point_varying_color_varying_outline_aa_frag.glsl
  shaders/gpu_shader_point_varying_color_frag.glsl
  shaders/gpu_shader_3D_point_fixed_size_varying_color_vert.glsl
  shaders/gpu_shader_3D_point_varying_size_varying_color_vert.glsl
  shaders/gpu_shader_3D_point_uniform_size_aa_vert.glsl
  shaders/gpu_shader_2D_point_varying_size_varying_color_vert.glsl
  shaders/gpu_shader_2D_point_uniform_size_aa_vert.glsl
  shaders/gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl

  shaders/gpu_shader_text_vert.glsl
  shaders/gpu_shader_text_frag.glsl
  shaders/gpu_shader_keyframe_shape_vert.glsl
  shaders/gpu_shader_keyframe_shape_frag.glsl

  shaders/gpu_shader_codegen_lib.glsl

  shaders/common/gpu_shader_common_color_ramp.glsl
  shaders/common/gpu_shader_common_color_utils.glsl
  shaders/common/gpu_shader_common_curves.glsl
  shaders/common/gpu_shader_common_hash.glsl
  shaders/common/gpu_shader_common_math.glsl
  shaders/common/gpu_shader_common_math_utils.glsl
  shaders/common/gpu_shader_common_mix_rgb.glsl

  shaders/material/gpu_shader_material_add_shader.glsl
  shaders/material/gpu_shader_material_ambient_occlusion.glsl
  shaders/material/gpu_shader_material_anisotropic.glsl
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
  shaders/material/gpu_shader_material_combine_hsv.glsl
  shaders/material/gpu_shader_material_combine_rgb.glsl
  shaders/material/gpu_shader_material_combine_xyz.glsl
  shaders/material/gpu_shader_material_diffuse.glsl
  shaders/material/gpu_shader_material_displacement.glsl
  shaders/material/gpu_shader_material_eevee_specular.glsl
  shaders/material/gpu_shader_material_emission.glsl
  shaders/material/gpu_shader_material_fractal_noise.glsl
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
  shaders/material/gpu_shader_material_refraction.glsl
  shaders/material/gpu_shader_material_rgb_to_bw.glsl
  shaders/material/gpu_shader_material_separate_color.glsl
  shaders/material/gpu_shader_material_separate_hsv.glsl
  shaders/material/gpu_shader_material_separate_rgb.glsl
  shaders/material/gpu_shader_material_separate_xyz.glsl
  shaders/material/gpu_shader_material_set.glsl
  shaders/material/gpu_shader_material_shader_to_rgba.glsl
  shaders/material/gpu_shader_material_squeeze.glsl
  shaders/material/gpu_shader_material_subsurface_scattering.glsl
  shaders/material/gpu_shader_material_tangent.glsl
  shaders/material/gpu_shader_material_tex_brick.glsl
  shaders/material/gpu_shader_material_tex_checker.glsl
  shaders/material/gpu_shader_material_tex_environment.glsl
  shaders/material/gpu_shader_material_tex_gradient.glsl
  shaders/material/gpu_shader_material_tex_image.glsl
  shaders/material/gpu_shader_material_tex_magic.glsl
  shaders/material/gpu_shader_material_tex_musgrave.glsl
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
  shaders/material/gpu_shader_material_velvet.glsl
  shaders/material/gpu_shader_material_vertex_color.glsl
  shaders/material/gpu_shader_material_volume_absorption.glsl
  shaders/material/gpu_shader_material_volume_principled.glsl
  shaders/material/gpu_shader_material_volume_scatter.glsl
  shaders/material/gpu_shader_material_wireframe.glsl
  shaders/material/gpu_shader_material_world_normals.glsl

  shaders/gpu_shader_gpencil_stroke_vert.glsl
  shaders/gpu_shader_gpencil_stroke_frag.glsl
  shaders/gpu_shader_gpencil_stroke_geom.glsl

  shaders/gpu_shader_cfg_world_clip_lib.glsl
  shaders/gpu_shader_colorspace_lib.glsl

  GPU_shader_shared_utils.h
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  data_to_c_simple(${GLSL_FILE} GLSL_C)
endforeach()


if(WITH_METAL_BACKEND)
  set(MSL_C)
  foreach(MSL_FILE ${MSL_SRC})
    data_to_c_simple(${MSL_FILE} MSL_C)
  endforeach()
  list(APPEND GLSL_C ${MSL_C})
endif()

blender_add_lib(bf_gpu_shaders "${GLSL_C}" "" "" "")

list(APPEND LIB
  bf_gpu_shaders
)

set(GLSL_SOURCE_CONTENT "")
foreach(GLSL_FILE ${GLSL_SRC})
  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(datatoc_${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
endforeach()

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_gpu_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

set(SRC_SHADER_CREATE_INFOS
  ../draw/engines/basic/shaders/infos/basic_depth_info.hh
  ../draw/engines/eevee_next/shaders/infos/eevee_material_info.hh
  ../draw/engines/eevee_next/shaders/infos/eevee_velocity_info.hh
  ../draw/engines/gpencil/shaders/infos/gpencil_info.hh
  ../draw/engines/gpencil/shaders/infos/gpencil_vfx_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_antialiasing_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_armature_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_background_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_edit_mode_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_extra_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_facing_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_grid_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_outline_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_paint_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_sculpt_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_volume_info.hh
  ../draw/engines/overlay/shaders/infos/overlay_wireframe_info.hh
  ../draw/engines/select/shaders/infos/select_id_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_composite_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_effect_antialiasing_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_effect_cavity_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_effect_dof_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_effect_outline_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_merge_infront_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_prepass_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_shadow_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_transparent_resolve_info.hh
  ../draw/engines/workbench/shaders/infos/workbench_volume_info.hh
  ../draw/engines/image/shaders/infos/engine_image_info.hh
  ../draw/intern/shaders/draw_fullscreen_info.hh
  ../draw/intern/shaders/draw_hair_refine_info.hh
  ../draw/intern/shaders/draw_object_infos_info.hh
  ../draw/intern/shaders/draw_view_info.hh

  shaders/infos/gpu_clip_planes_info.hh
  shaders/infos/gpu_shader_2D_area_borders_info.hh
  shaders/infos/gpu_shader_2D_checker_info.hh
  shaders/infos/gpu_shader_2D_diag_stripes_info.hh
  shaders/infos/gpu_shader_2D_flat_color_info.hh
  shaders/infos/gpu_shader_2D_image_color_info.hh
  shaders/infos/gpu_shader_2D_image_desaturate_color_info.hh
  shaders/infos/gpu_shader_2D_image_info.hh
  shaders/infos/gpu_shader_2D_image_multi_rect_color_info.hh
  shaders/infos/gpu_shader_2D_image_overlays_merge_info.hh
  shaders/infos/gpu_shader_2D_image_overlays_stereo_merge_info.hh
  shaders/infos/gpu_shader_2D_image_rect_color_info.hh
  shaders/infos/gpu_shader_2D_image_shuffle_color_info.hh
  shaders/infos/gpu_shader_2D_nodelink_info.hh
  shaders/infos/gpu_shader_2D_point_uniform_size_uniform_color_aa_info.hh
  shaders/infos/gpu_shader_2D_point_uniform_size_uniform_color_outline_aa_info.hh
  shaders/infos/gpu_shader_2D_point_varying_size_varying_color_info.hh
  shaders/infos/gpu_shader_2D_smooth_color_info.hh
  shaders/infos/gpu_shader_2D_uniform_color_info.hh
  shaders/infos/gpu_shader_2D_widget_info.hh
  shaders/infos/gpu_shader_3D_depth_only_info.hh
  shaders/infos/gpu_shader_3D_flat_color_info.hh
  shaders/infos/gpu_shader_3D_image_info.hh
  shaders/infos/gpu_shader_3D_image_modulate_alpha_info.hh
  shaders/infos/gpu_shader_3D_point_info.hh
  shaders/infos/gpu_shader_3D_polyline_info.hh
  shaders/infos/gpu_shader_3D_smooth_color_info.hh
  shaders/infos/gpu_shader_3D_uniform_color_info.hh
  shaders/infos/gpu_shader_gpencil_stroke_info.hh
  shaders/infos/gpu_shader_instance_varying_color_varying_size_info.hh
  shaders/infos/gpu_shader_keyframe_shape_info.hh
  shaders/infos/gpu_shader_line_dashed_uniform_color_info.hh
  shaders/infos/gpu_shader_simple_lighting_info.hh
  shaders/infos/gpu_shader_text_info.hh
  shaders/infos/gpu_srgb_to_framebuffer_space_info.hh
)

set(SHADER_CREATE_INFOS_CONTENT "")
foreach(DESCRIPTOR_FILE ${SRC_SHADER_CREATE_INFOS})
  string(APPEND SHADER_CREATE_INFOS_CONTENT "#include \"${DESCRIPTOR_FILE}\"\n")
endforeach()

set(shader_create_info_list_file "${CMAKE_CURRENT_BINARY_DIR}/gpu_shader_create_info_list.hh")
file(GENERATE OUTPUT ${shader_create_info_list_file} CONTENT "${SHADER_CREATE_INFOS_CONTENT}")

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

add_definitions(${GL_DEFINITIONS})

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

if(WITH_OPENCOLORIO)
  add_definitions(-DWITH_OCIO)
endif()

blender_add_lib(bf_gpu "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_gpu PUBLIC
  bf_draw_shaders
  bf_gpu_shaders
)

if(WITH_OPENCOLORIO)
  target_link_libraries(bf_gpu PUBLIC bf_ocio_shaders)
endif()


if(CXX_WARN_NO_SUGGEST_OVERRIDE)
  target_compile_options(bf_gpu PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>)
endif()



if(WITH_GPU_SHADER_BUILDER)
  # TODO(@fclem) Fix this mess.
  if(APPLE)
    add_executable(shader_builder
      intern/gpu_shader_builder.cc
      ${shader_create_info_list_file}
      )

    setup_platform_linker_flags(shader_builder)

    target_link_libraries(shader_builder PUBLIC
      bf_blenkernel
      buildinfoobj
    )
  else()
    if(WIN32)
      # We can re-use the manifest from tests.exe here since it's
      # rather generic and just selects the appropriate common
      # controls version.
      set(MANIFEST "${CMAKE_BINARY_DIR}/tests.exe.manifest")
    endif()
    add_executable(shader_builder
      intern/gpu_shader_builder.cc
      intern/gpu_shader_builder_stubs.cc
      ${shader_create_info_list_file}
      ${MANIFEST}
    )

    target_link_libraries(shader_builder PUBLIC
      bf_blenkernel
      ${PLATFORM_LINKLIBS}
    )
  endif()

  target_include_directories(shader_builder PRIVATE ${INC} ${CMAKE_CURRENT_BINARY_DIR})

  set(SRC_BAKED_CREATE_INFOS_FILE ${CMAKE_CURRENT_BINARY_DIR}/shader_baked.hh)

  add_custom_command(
    OUTPUT
    ${SRC_BAKED_CREATE_INFOS_FILE}
    COMMAND
      "$<TARGET_FILE:shader_builder>" ${SRC_BAKED_CREATE_INFOS_FILE}
    DEPENDS shader_builder
  )
  set(GPU_SHADER_INFO_SRC
    intern/gpu_shader_info_baked.cc
    ${SRC_BAKED_CREATE_INFOS_FILE}

    # For project files to be aware of these headers.
    ${SRC_SHADER_CREATE_INFOS}
    shaders/infos/gpu_interface_info.hh
  )

  blender_add_lib(bf_gpu_shader_infos "${GPU_SHADER_INFO_SRC}" "" "" "")
endif()


if(WITH_GTESTS)
  if(WITH_OPENGL_DRAW_TESTS)
    set(TEST_SRC
      tests/gpu_testing.cc

      tests/gpu_index_buffer_test.cc
      tests/gpu_shader_builtin_test.cc
      tests/gpu_shader_test.cc

      tests/gpu_testing.hh
    )
    set(TEST_INC
    )
    set(TEST_LIB
    )
    include(GTestTesting)
    blender_add_test_lib(bf_gpu_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
  endif()
endif()
