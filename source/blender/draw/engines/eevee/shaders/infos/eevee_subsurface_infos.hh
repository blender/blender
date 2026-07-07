/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_common_infos.hh"
#  include "eevee_uniform_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_subsurface_setup)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
TYPEDEF_SOURCE("draw_shader_shared.hh")
ADDITIONAL_INFO(draw_view)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
SAMPLER(2, sampler2DDepth, depth_tx)
IMAGE(0, DEFERRED_RADIANCE_FORMAT, read, uimage2D, direct_light_img)
IMAGE(1, RAYTRACE_RADIANCE_FORMAT, read, image2D, indirect_light_img)
IMAGE(2, SUBSURFACE_OBJECT_ID_FORMAT, write, uimage2D, object_id_img)
IMAGE(3, SUBSURFACE_RADIANCE_FORMAT, write, image2D, radiance_img)
STORAGE_BUF(0, write, uint, convolve_tile_buf[])
STORAGE_BUF(1, read_write, DispatchCommand, convolve_dispatch_buf)
COMPUTE_SOURCE("eevee_subsurface_setup_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_subsurface_convolve)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
ADDITIONAL_INFO(draw_view)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
ADDITIONAL_INFO(eevee_global_ubo)
SAMPLER(2, sampler2D, radiance_tx)
SAMPLER(3, sampler2DDepth, depth_tx)
SAMPLER(4, usampler2D, object_id_tx)
STORAGE_BUF(0, read, uint, tiles_coord_buf[])
IMAGE(0, DEFERRED_RADIANCE_FORMAT, write, uimage2D, out_direct_light_img)
IMAGE(1, RAYTRACE_RADIANCE_FORMAT, write, image2D, out_indirect_light_img)
COMPUTE_SOURCE("eevee_subsurface_convolve_comp.glsl")
GPU_SHADER_CREATE_END()
