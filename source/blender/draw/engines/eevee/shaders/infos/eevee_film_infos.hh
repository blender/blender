/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_film_shared.hh"
#  include "eevee_velocity_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_film_base)
SAMPLER(0, sampler2DDepth, depth_tx)
SAMPLER(1, sampler2D, combined_tx)
SAMPLER(2, sampler2D, vector_tx)
SAMPLER(3, sampler2DArray, rp_color_tx)
SAMPLER(4, sampler2DArray, rp_value_tx)
/* Color History for TAA needs to be sampler to leverage bilinear sampling. */
SAMPLER(5, sampler2D, in_combined_tx)
SAMPLER(6, sampler2D, cryptomatte_tx)
IMAGE(0, SFLOAT_32, read, image2DArray, in_weight_img)
IMAGE(1, SFLOAT_32, write, image2DArray, out_weight_img)
SPECIALIZATION_CONSTANT(uint, enabled_categories, 1)
SPECIALIZATION_CONSTANT(int, samples_len, 9)
SPECIALIZATION_CONSTANT(bool, use_reprojection, true)
SPECIALIZATION_CONSTANT(int, scaling_factor, 1)
SPECIALIZATION_CONSTANT(int, combined_id, 0)
SPECIALIZATION_CONSTANT(int, display_id, -1)
SPECIALIZATION_CONSTANT(int, normal_id, -1)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_velocity_camera)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film)
/* Color History for TAA needs to be sampler to leverage bilinear sampling. */
// IMAGE(2, SFLOAT_16_16_16_16, read, image2D, in_combined_img)
IMAGE(3, SFLOAT_16_16_16_16, write, image2D, out_combined_img)
IMAGE(4, SFLOAT_32, read_write, image2D, depth_img)
IMAGE(5, SFLOAT_16_16_16_16, read_write, image2DArray, color_accum_img)
IMAGE(6, SFLOAT_16, read_write, image2DArray, value_accum_img)
IMAGE(7, SFLOAT_32_32_32_32, read_write, image2DArray, cryptomatte_img)
ADDITIONAL_INFO(eevee_film_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_frag)
DO_STATIC_COMPILATION()
FRAGMENT_OUT(0, float4, out_color)
FRAGMENT_SOURCE("eevee_film_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(eevee_film)
DEPTH_WRITE(DepthWrite::ANY)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_comp)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
COMPUTE_SOURCE("eevee_film_comp.glsl")
ADDITIONAL_INFO(eevee_film)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_cryptomatte_post)
DO_STATIC_COMPILATION()
IMAGE(0, SFLOAT_32_32_32_32, read_write, image2DArray, cryptomatte_img)
PUSH_CONSTANT(int, cryptomatte_layer_len)
PUSH_CONSTANT(int, cryptomatte_samples_per_layer)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
COMPUTE_SOURCE("eevee_film_cryptomatte_post_comp.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
TYPEDEF_SOURCE("eevee_defines.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_copy_frag)
DO_STATIC_COMPILATION()
IMAGE(3, SFLOAT_16_16_16_16, read, image2D, out_combined_img)
IMAGE(4, SFLOAT_32, read, image2D, depth_img)
IMAGE(5, SFLOAT_16_16_16_16, read, image2DArray, color_accum_img)
IMAGE(6, SFLOAT_16, read, image2DArray, value_accum_img)
IMAGE(7, SFLOAT_32_32_32_32, read, image2DArray, cryptomatte_img)
DEPTH_WRITE(DepthWrite::ANY)
FRAGMENT_OUT(0, float4, out_color)
FRAGMENT_SOURCE("eevee_film_copy_frag.glsl")
DEFINE("FILM_COPY")
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(eevee_film_base)
GPU_SHADER_CREATE_END()

/* The combined pass is stored into its own 2D texture with a format of
 * SFLOAT_16_16_16_16. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_combined)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(int2, offset)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* The depth pass is stored into its own 2D texture with a format of
 * SFLOAT_32. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_depth)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(int2, offset)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_32, write, image2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Value passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_16. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_value)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(int2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, sampler2DArray, input_tx)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Color passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_16_16_16_16. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_color)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(int2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, sampler2DArray, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Cryptomatte passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_32_32_32_32. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_cryptomatte)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(int2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, sampler2DArray, input_tx)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
