/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "eevee_common_infos.hh"
#  include "eevee_light_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_shadow_infos.hh"
#  include "eevee_shadow_shared.hh"
#  include "eevee_uniform_infos.hh"
#  include "eevee_volume_infos.hh"

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO

#  define SHADOW_UPDATE_ATOMIC_RASTER
#  define MAT_TRANSPARENT

#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_atomic_iface, shadow_iface)
FLAT(int, shadow_view_id)
GPU_SHADER_NAMED_INTERFACE_END(shadow_iface)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_clipping_iface, shadow_clip)
SMOOTH(float3, position)
SMOOTH(float3, vector)
GPU_SHADER_NAMED_INTERFACE_END(shadow_clip)

GPU_SHADER_CREATE_INFO(eevee_surf_shadow)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(SHADOW_VIEW_MAX))
DEFINE("MAT_SHADOW")
TYPEDEF_SOURCE("eevee_shadow_shared.hh")
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
VERTEX_OUT(eevee_surf_shadow_clipping_iface)
STORAGE_BUF(SHADOW_RENDER_VIEW_BUF_SLOT, read, ShadowRenderView, render_view_buf[SHADOW_VIEW_MAX])
FRAGMENT_SOURCE("eevee_surf_shadow_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_atomic)
ADDITIONAL_INFO(eevee_surf_shadow)
DEFINE("SHADOW_UPDATE_ATOMIC_RASTER")
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
VERTEX_OUT(eevee_surf_shadow_atomic_iface)
STORAGE_BUF(SHADOW_RENDER_MAP_BUF_SLOT, read, uint, render_map_buf[SHADOW_RENDER_MAP_SIZE])
IMAGE(SHADOW_ATLAS_IMG_SLOT, UINT_32, read_write, uimage2DArrayAtomic, shadow_atlas_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_tbdr)
ADDITIONAL_INFO(eevee_surf_shadow)
DEFINE("SHADOW_UPDATE_TBDR")
BUILTINS(BuiltinBits::LAYER)
/* Use greater depth write to avoid loosing the early Z depth test but ensure correct fragment
 * ordering after slope bias. */
DEPTH_WRITE(DepthWrite::GREATER)
/* F32 color attachment for on-tile depth accumulation without atomics. */
FRAGMENT_OUT_ROG(0, float, out_depth, SHADOW_ROG_ID)
GPU_SHADER_CREATE_END()
