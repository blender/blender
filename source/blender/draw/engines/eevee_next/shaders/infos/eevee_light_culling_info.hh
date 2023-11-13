/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_data)
    .storage_buf(LIGHT_CULL_BUF_SLOT, Qualifier::READ, "LightCullingData", "light_cull_buf")
    .storage_buf(LIGHT_BUF_SLOT, Qualifier::READ, "LightData", "light_buf[]")
    .storage_buf(LIGHT_ZBIN_BUF_SLOT, Qualifier::READ, "uint", "light_zbin_buf[]")
    .storage_buf(LIGHT_TILE_BUF_SLOT, Qualifier::READ, "uint", "light_tile_buf[]");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Culling
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_culling_select)
    .do_static_compilation(true)
    .additional_info("eevee_shared", "draw_view", "draw_view_culling")
    .local_group_size(CULLING_SELECT_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ_WRITE, "LightCullingData", "light_cull_buf")
    .storage_buf(1, Qualifier::READ, "LightData", "in_light_buf[]")
    .storage_buf(2, Qualifier::WRITE, "LightData", "out_light_buf[]")
    .storage_buf(3, Qualifier::WRITE, "float", "out_zdist_buf[]")
    .storage_buf(4, Qualifier::WRITE, "uint", "out_key_buf[]")
    .compute_source("eevee_light_culling_select_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_light_culling_sort)
    .do_static_compilation(true)
    .additional_info("eevee_shared", "draw_view")
    .storage_buf(0, Qualifier::READ, "LightCullingData", "light_cull_buf")
    .storage_buf(1, Qualifier::READ, "LightData", "in_light_buf[]")
    .storage_buf(2, Qualifier::WRITE, "LightData", "out_light_buf[]")
    .storage_buf(3, Qualifier::READ, "float", "in_zdist_buf[]")
    .storage_buf(4, Qualifier::READ, "uint", "in_key_buf[]")
    .local_group_size(CULLING_SORT_GROUP_SIZE)
    .compute_source("eevee_light_culling_sort_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_light_culling_zbin)
    .do_static_compilation(true)
    .additional_info("eevee_shared", "draw_view")
    .local_group_size(CULLING_ZBIN_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ, "LightCullingData", "light_cull_buf")
    .storage_buf(1, Qualifier::READ, "LightData", "light_buf[]")
    .storage_buf(2, Qualifier::WRITE, "uint", "out_zbin_buf[]")
    .compute_source("eevee_light_culling_zbin_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_light_culling_tile)
    .do_static_compilation(true)
    .additional_info("eevee_shared", "draw_view", "draw_view_culling")
    .local_group_size(CULLING_TILE_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ, "LightCullingData", "light_cull_buf")
    .storage_buf(1, Qualifier::READ, "LightData", "light_buf[]")
    .storage_buf(2, Qualifier::WRITE, "uint", "out_light_tile_buf[]")
    .compute_source("eevee_light_culling_tile_comp.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_light_culling_debug)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_debug_color_add", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_debug_color_mul", DualBlend::SRC_1)
    .fragment_source("eevee_light_culling_debug_frag.glsl")
    .additional_info(
        "eevee_shared", "draw_view", "draw_fullscreen", "eevee_light_data", "eevee_hiz_data");

/** \} */
