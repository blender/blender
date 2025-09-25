/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_hiz_update_base)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
STORAGE_BUF(0, read_write, uint, finished_tile_counter)
IMAGE(0, SFLOAT_32, write, image2D, out_mip_0)
IMAGE(1, SFLOAT_32, write, image2D, out_mip_1)
IMAGE(2, SFLOAT_32, write, image2D, out_mip_2)
IMAGE(3, SFLOAT_32, write, image2D, out_mip_3)
IMAGE(4, SFLOAT_32, write, image2D, out_mip_4)
IMAGE(5, SFLOAT_32, read_write, image2D, out_mip_5)
IMAGE(6, SFLOAT_32, write, image2D, out_mip_6)
SPECIALIZATION_CONSTANT(bool, update_mip_0, true)
COMPUTE_SOURCE("eevee_hiz_update_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_update)
DO_STATIC_COMPILATION()
SAMPLER(0, sampler2DDepth, depth_tx)
ADDITIONAL_INFO(eevee_hiz_update_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_update_layer)
DO_STATIC_COMPILATION()
DEFINE("HIZ_LAYER")
SAMPLER(1, sampler2DArrayDepth, depth_layered_tx)
PUSH_CONSTANT(int, layer_id)
ADDITIONAL_INFO(eevee_hiz_update_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_debug)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, float4, out_debug_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_debug_color_mul, SRC_1)
FRAGMENT_SOURCE("eevee_hiz_debug_frag.glsl")
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()
