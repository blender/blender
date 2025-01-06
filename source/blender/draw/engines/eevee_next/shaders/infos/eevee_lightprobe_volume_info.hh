/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"
#  include "eevee_common_info.hh"
#  include "eevee_shader_shared.hh"

#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_debug_surfel_iface)
SMOOTH(VEC3, P)
FLAT(INT, surfel_index)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_surfels)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
VERTEX_SOURCE("eevee_debug_surfels_vert.glsl")
VERTEX_OUT(eevee_debug_surfel_iface)
FRAGMENT_SOURCE("eevee_debug_surfels_frag.glsl")
FRAGMENT_OUT(0, VEC4, out_color)
STORAGE_BUF(0, READ, Surfel, surfels_buf[])
PUSH_CONSTANT(FLOAT, debug_surfel_radius)
PUSH_CONSTANT(INT, debug_mode)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_debug_irradiance_grid_iface)
SMOOTH(VEC4, interp_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
FRAGMENT_OUT(0, VEC4, out_color)
VERTEX_OUT(eevee_debug_irradiance_grid_iface)
SAMPLER(0, FLOAT_3D, debug_data_tx)
PUSH_CONSTANT(MAT4, grid_mat)
PUSH_CONSTANT(INT, debug_mode)
PUSH_CONSTANT(FLOAT, debug_value)
VERTEX_SOURCE("eevee_debug_irradiance_grid_vert.glsl")
FRAGMENT_SOURCE("eevee_debug_irradiance_grid_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_volume_iface)
SMOOTH(VEC2, lP)
FLAT(IVEC3, cell)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_volume)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
VERTEX_SOURCE("eevee_display_lightprobe_volume_vert.glsl")
VERTEX_OUT(eevee_display_lightprobe_volume_iface)
FRAGMENT_SOURCE("eevee_display_lightprobe_volume_frag.glsl")
FRAGMENT_OUT(0, VEC4, out_color)
PUSH_CONSTANT(FLOAT, sphere_radius)
PUSH_CONSTANT(IVEC3, grid_resolution)
PUSH_CONSTANT(MAT4, grid_to_world)
PUSH_CONSTANT(MAT4, world_to_grid)
PUSH_CONSTANT(BOOL, display_validity)
SAMPLER(0, FLOAT_3D, irradiance_a_tx)
SAMPLER(1, FLOAT_3D, irradiance_b_tx)
SAMPLER(2, FLOAT_3D, irradiance_c_tx)
SAMPLER(3, FLOAT_3D, irradiance_d_tx)
SAMPLER(4, FLOAT_3D, validity_tx)
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
ADDITIONAL_INFO(eevee_shared)
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
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
IMAGE(0, GPU_R32I, READ_WRITE, INT_3D_ATOMIC, cluster_list_img)
COMPUTE_SOURCE("eevee_surfel_cluster_build_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_build)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, READ_WRITE, int, list_start_buf[])
STORAGE_BUF(6, READ_WRITE, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_build_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_list_sort)
LOCAL_GROUP_SIZE(SURFEL_LIST_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, READ_WRITE, int, list_start_buf[])
STORAGE_BUF(6, READ, SurfelListInfoData, list_info_buf)
COMPUTE_SOURCE("eevee_surfel_list_sort_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_ray)
LOCAL_GROUP_SIZE(SURFEL_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(draw_view)
PUSH_CONSTANT(INT, radiance_src)
PUSH_CONSTANT(INT, radiance_dst)
COMPUTE_SOURCE("eevee_surfel_ray_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_bounds)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(IRRADIANCE_BOUNDS_GROUP_SIZE)
STORAGE_BUF(0, READ_WRITE, CaptureInfoData, capture_info_buf)
STORAGE_BUF(1, READ, ObjectBounds, bounds_buf[])
PUSH_CONSTANT(INT, resource_len)
TYPEDEF_SOURCE("draw_shader_shared.hh")
ADDITIONAL_INFO(eevee_shared)
COMPUTE_SOURCE("eevee_lightprobe_volume_bounds_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_ray)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(draw_view)
PUSH_CONSTANT(INT, radiance_src)
STORAGE_BUF(0, READ, int, list_start_buf[])
STORAGE_BUF(6, READ, SurfelListInfoData, list_info_buf)
IMAGE(0, GPU_RGBA32F, READ_WRITE, FLOAT_3D, irradiance_L0_img)
IMAGE(1, GPU_RGBA32F, READ_WRITE, FLOAT_3D, irradiance_L1_a_img)
IMAGE(2, GPU_RGBA32F, READ_WRITE, FLOAT_3D, irradiance_L1_b_img)
IMAGE(3, GPU_RGBA32F, READ_WRITE, FLOAT_3D, irradiance_L1_c_img)
IMAGE(4, GPU_RGBA16F, READ, FLOAT_3D, virtual_offset_img)
IMAGE(5, GPU_R32F, READ_WRITE, FLOAT_3D, validity_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_ray_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_offset)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE,
                 IRRADIANCE_GRID_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_surfel_common)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, READ, int, list_start_buf[])
STORAGE_BUF(6, READ, SurfelListInfoData, list_info_buf)
IMAGE(0, GPU_R32I, READ, INT_3D_ATOMIC, cluster_list_img)
IMAGE(1, GPU_RGBA16F, READ_WRITE, FLOAT_3D, virtual_offset_img)
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
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_global_ubo)
PUSH_CONSTANT(INT, grid_index)
STORAGE_BUF(0, READ, uint, bricks_infos_buf[])
STORAGE_BUF(1, READ, SphereProbeHarmonic, harmonic_buf)
UNIFORM_BUF(0, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
IMAGE(0, VOLUME_PROBE_FORMAT, WRITE, FLOAT_3D, irradiance_atlas_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_world_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_volume_load)
LOCAL_GROUP_SIZE(IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE,
                 IRRADIANCE_GRID_BRICK_SIZE)
DEFINE("IRRADIANCE_GRID_UPLOAD")
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_global_ubo)
PUSH_CONSTANT(MAT4, grid_local_to_world)
PUSH_CONSTANT(INT, grid_index)
PUSH_CONSTANT(INT, grid_start_index)
PUSH_CONSTANT(FLOAT, validity_threshold)
PUSH_CONSTANT(FLOAT, dilation_threshold)
PUSH_CONSTANT(FLOAT, dilation_radius)
PUSH_CONSTANT(FLOAT, grid_intensity_factor)
UNIFORM_BUF(0, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
STORAGE_BUF(0, READ, uint, bricks_infos_buf[])
SAMPLER(0, FLOAT_3D, irradiance_a_tx)
SAMPLER(1, FLOAT_3D, irradiance_b_tx)
SAMPLER(2, FLOAT_3D, irradiance_c_tx)
SAMPLER(3, FLOAT_3D, irradiance_d_tx)
SAMPLER(4, FLOAT_3D, visibility_a_tx)
SAMPLER(5, FLOAT_3D, visibility_b_tx)
SAMPLER(6, FLOAT_3D, visibility_c_tx)
SAMPLER(7, FLOAT_3D, visibility_d_tx)
SAMPLER(8, FLOAT_3D, irradiance_atlas_tx)
SAMPLER(9, FLOAT_3D, validity_tx)
IMAGE(0, VOLUME_PROBE_FORMAT, WRITE, FLOAT_3D, irradiance_atlas_img)
COMPUTE_SOURCE("eevee_lightprobe_volume_load_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
