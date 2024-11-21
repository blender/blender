/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_data)
STORAGE_BUF(LIGHT_CULL_BUF_SLOT, READ, LightCullingData, light_cull_buf)
STORAGE_BUF(LIGHT_BUF_SLOT, READ, LightData, light_buf[])
STORAGE_BUF(LIGHT_ZBIN_BUF_SLOT, READ, uint, light_zbin_buf[])
STORAGE_BUF(LIGHT_TILE_BUF_SLOT, READ, uint, light_tile_buf[])
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Culling
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_culling_select)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_view_culling)
LOCAL_GROUP_SIZE(CULLING_SELECT_GROUP_SIZE)
STORAGE_BUF(0, READ_WRITE, LightCullingData, light_cull_buf)
STORAGE_BUF(1, READ, LightData, in_light_buf[])
STORAGE_BUF(2, WRITE, LightData, out_light_buf[])
STORAGE_BUF(3, WRITE, float, out_zdist_buf[])
STORAGE_BUF(4, WRITE, uint, out_key_buf[])
UNIFORM_BUF(0, LightData, sunlight_buf)
COMPUTE_SOURCE("eevee_light_culling_select_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_light_culling_sort)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
STORAGE_BUF(0, READ, LightCullingData, light_cull_buf)
STORAGE_BUF(1, READ, LightData, in_light_buf[])
STORAGE_BUF(2, WRITE, LightData, out_light_buf[])
STORAGE_BUF(3, READ, float, in_zdist_buf[])
STORAGE_BUF(4, READ, uint, in_key_buf[])
LOCAL_GROUP_SIZE(CULLING_SORT_GROUP_SIZE)
COMPUTE_SOURCE("eevee_light_culling_sort_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_light_culling_zbin)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
LOCAL_GROUP_SIZE(CULLING_ZBIN_GROUP_SIZE)
STORAGE_BUF(0, READ, LightCullingData, light_cull_buf)
STORAGE_BUF(1, READ, LightData, light_buf[])
STORAGE_BUF(2, WRITE, uint, out_zbin_buf[])
COMPUTE_SOURCE("eevee_light_culling_zbin_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_light_culling_tile)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_view_culling)
LOCAL_GROUP_SIZE(CULLING_TILE_GROUP_SIZE)
STORAGE_BUF(0, READ, LightCullingData, light_cull_buf)
STORAGE_BUF(1, READ, LightData, light_buf[])
STORAGE_BUF(2, WRITE, uint, out_light_tile_buf[])
COMPUTE_SOURCE("eevee_light_culling_tile_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_light_shadow_setup)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_global_ubo)
LOCAL_GROUP_SIZE(CULLING_SELECT_GROUP_SIZE)
STORAGE_BUF(0, READ, LightCullingData, light_cull_buf)
STORAGE_BUF(1, READ_WRITE, LightData, light_buf[])
STORAGE_BUF(2, READ_WRITE, ShadowTileMapData, tilemaps_buf[])
STORAGE_BUF(3, READ_WRITE, ShadowTileMapClip, tilemaps_clip_buf[])
COMPUTE_SOURCE("eevee_light_shadow_setup_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_culling_debug)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, VEC4, out_debug_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, out_debug_color_mul, SRC_1)
FRAGMENT_SOURCE("eevee_light_culling_debug_frag.glsl")
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_hiz_data)
GPU_SHADER_CREATE_END()

/** \} */
