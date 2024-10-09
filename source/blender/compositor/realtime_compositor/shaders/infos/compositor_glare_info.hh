/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------
 * Common.
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_highlights)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, threshold)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_glare_highlights.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_mix)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, mix_factor)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, glare_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_glare_mix.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* ------------
 * Ghost Glare.
 * ------------ */

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_base)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, small_ghost_tx)
SAMPLER(1, FLOAT_2D, big_ghost_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, combined_ghost_img)
COMPUTE_SOURCE("compositor_glare_ghost_base.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_accumulate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(VEC4, scales)
PUSH_CONSTANT_ARRAY(VEC4, color_modulators, 4)
SAMPLER(0, FLOAT_2D, input_ghost_tx)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, accumulated_ghost_img)
COMPUTE_SOURCE("compositor_glare_ghost_accumulate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -----------
 * Simple Star
 * ----------- */

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_horizontal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(INT, iterations)
PUSH_CONSTANT(FLOAT, fade_factor)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, horizontal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_horizontal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_vertical_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(INT, iterations)
PUSH_CONSTANT(FLOAT, fade_factor)
SAMPLER(0, FLOAT_2D, horizontal_tx)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, vertical_img)
COMPUTE_SOURCE("compositor_glare_simple_star_vertical_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_diagonal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(INT, iterations)
PUSH_CONSTANT(FLOAT, fade_factor)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, diagonal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_diagonal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_anti_diagonal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(INT, iterations)
PUSH_CONSTANT(FLOAT, fade_factor)
SAMPLER(0, FLOAT_2D, diagonal_tx)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, anti_diagonal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_anti_diagonal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -------
 * Streaks
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_filter)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, color_modulator)
PUSH_CONSTANT(VEC3, fade_factors)
PUSH_CONSTANT(VEC2, streak_vector)
SAMPLER(0, FLOAT_2D, input_streak_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_streak_img)
COMPUTE_SOURCE("compositor_glare_streaks_filter.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_accumulate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, attenuation_factor)
SAMPLER(0, FLOAT_2D, streak_tx)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, accumulated_streaks_img)
COMPUTE_SOURCE("compositor_glare_streaks_accumulate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -----
 * Bloom
 * ----- */

GPU_SHADER_CREATE_INFO(compositor_glare_bloom_downsample_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_glare_bloom_downsample.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_bloom_downsample_simple_average)
DEFINE("SIMPLE_AVERAGE")
ADDITIONAL_INFO(compositor_glare_bloom_downsample_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_bloom_downsample_karis_average)
DEFINE("KARIS_AVERAGE")
ADDITIONAL_INFO(compositor_glare_bloom_downsample_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_bloom_upsample)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_glare_bloom_upsample.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
