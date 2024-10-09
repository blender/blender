/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_hiz_data)
SAMPLER(HIZ_TEX_SLOT, FLOAT_2D, hiz_tx)
ADDITIONAL_INFO(eevee_global_ubo)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_update_base)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
STORAGE_BUF(0, READ_WRITE, uint, finished_tile_counter)
IMAGE(0, GPU_R32F, WRITE, FLOAT_2D, out_mip_0)
IMAGE(1, GPU_R32F, WRITE, FLOAT_2D, out_mip_1)
IMAGE(2, GPU_R32F, WRITE, FLOAT_2D, out_mip_2)
IMAGE(3, GPU_R32F, WRITE, FLOAT_2D, out_mip_3)
IMAGE(4, GPU_R32F, WRITE, FLOAT_2D, out_mip_4)
IMAGE(5, GPU_R32F, READ_WRITE, FLOAT_2D, out_mip_5)
IMAGE(6, GPU_R32F, WRITE, FLOAT_2D, out_mip_6)
SPECIALIZATION_CONSTANT(BOOL, update_mip_0, true)
COMPUTE_SOURCE("eevee_hiz_update_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_update)
DO_STATIC_COMPILATION()
SAMPLER(0, DEPTH_2D, depth_tx)
ADDITIONAL_INFO(eevee_hiz_update_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_update_layer)
DO_STATIC_COMPILATION()
DEFINE("HIZ_LAYER")
SAMPLER(1, DEPTH_2D_ARRAY, depth_layered_tx)
PUSH_CONSTANT(INT, layer_id)
ADDITIONAL_INFO(eevee_hiz_update_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_debug)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, VEC4, out_debug_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, out_debug_color_mul, SRC_1)
FRAGMENT_SOURCE("eevee_hiz_debug_frag.glsl")
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(draw_fullscreen)
GPU_SHADER_CREATE_END()
