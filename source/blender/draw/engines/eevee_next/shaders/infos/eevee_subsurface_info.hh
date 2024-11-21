/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_subsurface_setup)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
TYPEDEF_SOURCE("draw_shader_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_gbuffer_data)
SAMPLER(2, DEPTH_2D, depth_tx)
IMAGE(0, DEFERRED_RADIANCE_FORMAT, READ, UINT_2D, direct_light_img)
IMAGE(1, RAYTRACE_RADIANCE_FORMAT, READ, FLOAT_2D, indirect_light_img)
IMAGE(2, SUBSURFACE_OBJECT_ID_FORMAT, WRITE, UINT_2D, object_id_img)
IMAGE(3, SUBSURFACE_RADIANCE_FORMAT, WRITE, FLOAT_2D, radiance_img)
STORAGE_BUF(0, WRITE, uint, convolve_tile_buf[])
STORAGE_BUF(1, READ_WRITE, DispatchCommand, convolve_dispatch_buf)
COMPUTE_SOURCE("eevee_subsurface_setup_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_subsurface_convolve)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_gbuffer_data)
ADDITIONAL_INFO(eevee_global_ubo)
SAMPLER(2, FLOAT_2D, radiance_tx)
SAMPLER(3, DEPTH_2D, depth_tx)
SAMPLER(4, UINT_2D, object_id_tx)
STORAGE_BUF(0, READ, uint, tiles_coord_buf[])
IMAGE(0, DEFERRED_RADIANCE_FORMAT, WRITE, UINT_2D, out_direct_light_img)
IMAGE(1, RAYTRACE_RADIANCE_FORMAT, WRITE, FLOAT_2D, out_indirect_light_img)
COMPUTE_SOURCE("eevee_subsurface_convolve_comp.glsl")
GPU_SHADER_CREATE_END()
