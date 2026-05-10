/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_raytrace_shared.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_uniform_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Ray tracing Pipeline
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_ray_tile_classify)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
ADDITIONAL_INFO(eevee_global_ubo)
TYPEDEF_SOURCE("draw_shader_shared.hh")
IMAGE_FREQ(0, RAYTRACE_TILEMASK_FORMAT, write, uimage2DArray, tile_raytrace_denoise_img, PASS)
IMAGE_FREQ(1, RAYTRACE_TILEMASK_FORMAT, write, uimage2DArray, tile_raytrace_tracing_img, PASS)
IMAGE_FREQ(2, RAYTRACE_TILEMASK_FORMAT, write, uimage2DArray, tile_fast_gi_denoise_img, PASS)
IMAGE_FREQ(3, RAYTRACE_TILEMASK_FORMAT, write, uimage2DArray, tile_fast_gi_tracing_img, PASS)
COMPUTE_SOURCE("eevee_ray_tile_classify_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_ray_tile_compact)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
TYPEDEF_SOURCE("draw_shader_shared.hh")
IMAGE_FREQ(0, RAYTRACE_TILEMASK_FORMAT, read, uimage2DArray, tile_raytrace_denoise_img, PASS)
IMAGE_FREQ(1, RAYTRACE_TILEMASK_FORMAT, read, uimage2DArray, tile_raytrace_tracing_img, PASS)
STORAGE_BUF(0, read_write, DispatchCommand, raytrace_tracing_dispatch_buf)
STORAGE_BUF(1, read_write, DispatchCommand, raytrace_denoise_dispatch_buf)
STORAGE_BUF(4, write, uint, raytrace_tracing_tiles_buf[])
STORAGE_BUF(5, write, uint, raytrace_denoise_tiles_buf[])
SPECIALIZATION_CONSTANT(int, closure_index, 0)
SPECIALIZATION_CONSTANT(int, resolution_scale, 2)
COMPUTE_SOURCE("eevee_ray_tile_compact_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_ray_generate)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_utility_texture)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, out_ray_data_img)
STORAGE_BUF(4, read, uint, tiles_coord_buf[])
SPECIALIZATION_CONSTANT(int, closure_index, 0)
COMPUTE_SOURCE("eevee_ray_generate_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */
