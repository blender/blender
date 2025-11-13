/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_debug_shared.hh"
#  include "eevee_light_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_lightprobe_shared.hh"
#  include "eevee_shadow_infos.hh"
#  include "eevee_uniform_infos.hh"

#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_debug_surfel_iface)
SMOOTH(float3, P)
FLAT(int, surfel_index)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_surfels)
TYPEDEF_SOURCE("draw_shader_shared.hh")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_debug_shared.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
VERTEX_SOURCE("eevee_debug_surfels_vert.glsl")
VERTEX_OUT(eevee_debug_surfel_iface)
FRAGMENT_SOURCE("eevee_debug_surfels_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
STORAGE_BUF(0, read, Surfel, surfels_buf[])
PUSH_CONSTANT(float, debug_surfel_radius)
PUSH_CONSTANT(int, debug_mode)
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_debug_irradiance_grid_iface)
SMOOTH(float4, interp_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_debug_shared.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
FRAGMENT_OUT(0, float4, out_color)
VERTEX_OUT(eevee_debug_irradiance_grid_iface)
SAMPLER(0, sampler3D, debug_data_tx)
PUSH_CONSTANT(float4x4, grid_mat)
PUSH_CONSTANT(int, debug_mode)
PUSH_CONSTANT(float, debug_value)
VERTEX_SOURCE("eevee_debug_irradiance_grid_vert.glsl")
FRAGMENT_SOURCE("eevee_debug_irradiance_grid_frag.glsl")
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_volume_iface)
SMOOTH(float2, lP)
FLAT(int3, cell)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_volume)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
VERTEX_SOURCE("eevee_display_lightprobe_volume_vert.glsl")
VERTEX_OUT(eevee_display_lightprobe_volume_iface)
FRAGMENT_SOURCE("eevee_display_lightprobe_volume_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
PUSH_CONSTANT(float, sphere_radius)
PUSH_CONSTANT(int3, grid_resolution)
PUSH_CONSTANT(float4x4, grid_to_world)
PUSH_CONSTANT(float4x4, world_to_grid)
PUSH_CONSTANT(bool, display_validity)
SAMPLER(0, sampler3D, irradiance_a_tx)
SAMPLER(1, sampler3D, irradiance_b_tx)
SAMPLER(2, sampler3D, irradiance_c_tx)
SAMPLER(3, sampler3D, irradiance_d_tx)
SAMPLER(4, sampler3D, validity_tx)
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Baking
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surfel_light)
DEFINE("LIGHT_ITER_FORCE_NO_CULLING")
DEFINE_VALUE("LIGHT_CLOSURE_EVAL_COUNT", "1")
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_shadow_data)
COMPUTE_SOURCE("eevee_surfel_light_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_cluster_build)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
IMAGE(0, SINT_32, read_write, iimage3DAtomic, cluster_list_img)
COMPUTE_SOURCE("eevee_surfel_cluster_build_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_prepare)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, read_write, int, list_counter_buf[])
STORAGE_BUF(6, read_write, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_prepare_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_prefix)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, read, int, list_counter_buf[])
STORAGE_BUF(2, write, int, list_range_buf[])
STORAGE_BUF(6, read_write, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_prefix_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_flatten)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, read_write, int, list_counter_buf[])
STORAGE_BUF(1, read, int, list_range_buf[])
STORAGE_BUF(2, write, float, list_item_distance_buf[])
STORAGE_BUF(3, write, int, list_item_surfel_id_buf[])
STORAGE_BUF(6, read, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_flatten_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_sort)
LOCAL_GROUP_SIZE(SURFEL_LIST_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, read, int, list_range_buf[])
STORAGE_BUF(1, read, int, list_item_surfel_id_buf[])
STORAGE_BUF(2, read, float, list_item_distance_buf[])
STORAGE_BUF(3, write, int, sorted_surfel_id_buf[])
STORAGE_BUF(6, read, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_sort_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_build)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, write, int, list_start_buf[])
STORAGE_BUF(1, read, int, list_range_buf[])
STORAGE_BUF(3, read, int, sorted_surfel_id_buf[])
STORAGE_BUF(6, read_write, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_build_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_ray)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(draw_view)
PUSH_CONSTANT(int, radiance_src)
PUSH_CONSTANT(int, radiance_dst)
COMPUTE_SOURCE("eevee_surfel_ray_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_bounds)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(IRRADIANCE_BOUNDS_GROUP_SIZE)
STORAGE_BUF(0, read_write, CaptureInfoData, capture_info_buf)
STORAGE_BUF(1, read, ObjectBounds, bounds_buf[])
PUSH_CONSTANT(int, resource_len)
TYPEDEF_SOURCE("draw_shader_shared.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
TYPEDEF_SOURCE("eevee_defines.hh")
COMPUTE_SOURCE("eevee_lightprobe_volume_bounds_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_ray)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(draw_view)
PUSH_CONSTANT(int, radiance_src)
STORAGE_BUF(0, read, int, list_start_buf[])
STORAGE_BUF(6, read, SurfelListInfoData, list_info_buf)
IMAGE(0, SFLOAT_32_32_32_32, read_write, image3D, irradiance_L0_img)
IMAGE(1, SFLOAT_32_32_32_32, read_write, image3D, irradiance_L1_a_img)
IMAGE(2, SFLOAT_32_32_32_32, read_write, image3D, irradiance_L1_b_img)
IMAGE(3, SFLOAT_32_32_32_32, read_write, image3D, irradiance_L1_c_img)
IMAGE(4, SFLOAT_16_16_16_16, read, image3D, virtual_offset_img)
IMAGE(5, SFLOAT_32, read_write, image3D, validity_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_ray_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_offset)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, read, int, list_start_buf[])
STORAGE_BUF(6, read, SurfelListInfoData, list_info_buf)
IMAGE(0, SINT_32, read, iimage3DAtomic, cluster_list_img)
IMAGE(1, SFLOAT_16_16_16_16, read_write, image3D, virtual_offset_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_offset_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Runtime
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_world)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE)
DEFINE("IRRADIANCE_GRID_UPLOAD")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_global_ubo)
PUSH_CONSTANT(int, grid_index)
STORAGE_BUF(0, read, uint, bricks_infos_buf[])
STORAGE_BUF(1, read, SphereProbeHarmonic, harmonic_buf)
UNIFORM_BUF(0, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
IMAGE(0, VOLUME_PROBE_FORMAT, write, image3D, irradiance_atlas_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_world_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_load)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE)
DEFINE("IRRADIANCE_GRID_UPLOAD")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_global_ubo)
PUSH_CONSTANT(float4x4, grid_local_to_world)
PUSH_CONSTANT(int, grid_index)
PUSH_CONSTANT(int, grid_start_index)
PUSH_CONSTANT(float, validity_threshold)
PUSH_CONSTANT(float, dilation_threshold)
PUSH_CONSTANT(float, dilation_radius)
PUSH_CONSTANT(float, grid_intensity_factor)
UNIFORM_BUF(0, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
STORAGE_BUF(0, read, uint, bricks_infos_buf[])
SAMPLER(0, sampler3D, irradiance_a_tx)
SAMPLER(1, sampler3D, irradiance_b_tx)
SAMPLER(2, sampler3D, irradiance_c_tx)
SAMPLER(3, sampler3D, irradiance_d_tx)
SAMPLER(4, sampler3D, visibility_a_tx)
SAMPLER(5, sampler3D, visibility_b_tx)
SAMPLER(6, sampler3D, visibility_c_tx)
SAMPLER(7, sampler3D, visibility_d_tx)
SAMPLER(8, sampler3D, irradiance_atlas_tx)
SAMPLER(9, sampler3D, validity_tx)
IMAGE(0, VOLUME_PROBE_FORMAT, write, image3D, irradiance_atlas_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_load_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
