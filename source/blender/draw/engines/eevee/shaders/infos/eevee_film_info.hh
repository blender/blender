/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_fullscreen_info.hh"
#  include "draw_view_info.hh"
#  include "eevee_common_info.hh"
#  include "eevee_shader_shared.hh"
#  include "eevee_velocity_info.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_film_base)
SAMPLER(0, DEPTH_2D, depth_tx)
SAMPLER(1, FLOAT_2D, combined_tx)
SAMPLER(2, FLOAT_2D, vector_tx)
SAMPLER(3, FLOAT_2D_ARRAY, rp_color_tx)
SAMPLER(4, FLOAT_2D_ARRAY, rp_value_tx)
/* Color History for TAA needs to be sampler to leverage bilinear sampling. */
SAMPLER(5, FLOAT_2D, in_combined_tx)
SAMPLER(6, FLOAT_2D, cryptomatte_tx)
IMAGE(0, GPU_R32F, READ, FLOAT_2D_ARRAY, in_weight_img)
IMAGE(1, GPU_R32F, WRITE, FLOAT_2D_ARRAY, out_weight_img)
SPECIALIZATION_CONSTANT(UINT, enabled_categories, 0)
SPECIALIZATION_CONSTANT(INT, samples_len, 0)
SPECIALIZATION_CONSTANT(BOOL, use_reprojection, false)
SPECIALIZATION_CONSTANT(INT, scaling_factor, 1)
SPECIALIZATION_CONSTANT(INT, combined_id, 0)
SPECIALIZATION_CONSTANT(INT, display_id, -1)
SPECIALIZATION_CONSTANT(INT, normal_id, -1)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_velocity_camera)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film)
/* Color History for TAA needs to be sampler to leverage bilinear sampling. */
// IMAGE(2, GPU_RGBA16F, READ, FLOAT_2D, in_combined_img)
IMAGE(3, GPU_RGBA16F, WRITE, FLOAT_2D, out_combined_img)
IMAGE(4, GPU_R32F, READ_WRITE, FLOAT_2D, depth_img)
IMAGE(5, GPU_RGBA16F, READ_WRITE, FLOAT_2D_ARRAY, color_accum_img)
IMAGE(6, GPU_R16F, READ_WRITE, FLOAT_2D_ARRAY, value_accum_img)
IMAGE(7, GPU_RGBA32F, READ_WRITE, FLOAT_2D_ARRAY, cryptomatte_img)
ADDITIONAL_INFO(eevee_film_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_frag)
DO_STATIC_COMPILATION()
FRAGMENT_OUT(0, VEC4, out_color)
FRAGMENT_SOURCE("eevee_film_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
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
IMAGE(0, GPU_RGBA32F, READ_WRITE, FLOAT_2D_ARRAY, cryptomatte_img)
PUSH_CONSTANT(INT, cryptomatte_layer_len)
PUSH_CONSTANT(INT, cryptomatte_samples_per_layer)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
COMPUTE_SOURCE("eevee_film_cryptomatte_post_comp.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_shared)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_film_copy_frag)
DO_STATIC_COMPILATION()
IMAGE(3, GPU_RGBA16F, READ, FLOAT_2D, out_combined_img)
IMAGE(4, GPU_R32F, READ, FLOAT_2D, depth_img)
IMAGE(5, GPU_RGBA16F, READ, FLOAT_2D_ARRAY, color_accum_img)
IMAGE(6, GPU_R16F, READ, FLOAT_2D_ARRAY, value_accum_img)
IMAGE(7, GPU_RGBA32F, READ, FLOAT_2D_ARRAY, cryptomatte_img)
DEPTH_WRITE(DepthWrite::ANY)
FRAGMENT_OUT(0, VEC4, out_color)
FRAGMENT_SOURCE("eevee_film_copy_frag.glsl")
DEFINE("FILM_COPY")
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(eevee_film_base)
GPU_SHADER_CREATE_END()

/* The combined pass is stored into its own 2D texture with a format of GPU_RGBA16F. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_combined)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(IVEC2, offset)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* The depth pass is stored into its own 2D texture with a format of GPU_R32F. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_depth)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(IVEC2, offset)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_R32F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Value passes are stored in a slice of a 2D texture array with a format of GPU_R16F. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_value)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(IVEC2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, FLOAT_2D_ARRAY, input_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Color passes are stored in a slice of a 2D texture array with a format of GPU_RGBA16F. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_color)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(IVEC2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, FLOAT_2D_ARRAY, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Cryptomatte passes are stored in a slice of a 2D texture array with a format of GPU_RGBA32F. */
GPU_SHADER_CREATE_INFO(eevee_film_pass_convert_cryptomatte)
LOCAL_GROUP_SIZE(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
PUSH_CONSTANT(IVEC2, offset)
DEFINE("IS_ARRAY_INPUT")
SAMPLER(0, FLOAT_2D_ARRAY, input_tx)
IMAGE(0, GPU_RGBA32F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("eevee_film_pass_convert_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
