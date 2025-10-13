/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------
 * Common.
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_highlights)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, threshold)
PUSH_CONSTANT(float, highlights_smoothness)
PUSH_CONSTANT(float, max_brightness)
PUSH_CONSTANT(int, quality)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_highlights.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_mix)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, saturation)
PUSH_CONSTANT(float3, tint)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, glare_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_mix.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_write_glare_output)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, saturation)
PUSH_CONSTANT(float3, tint)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_write_glare_output.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_write_highlights_output)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_write_highlights_output.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* ------------
 * Ghost Glare.
 * ------------ */

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_base)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, small_ghost_tx)
SAMPLER(1, sampler2D, big_ghost_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, combined_ghost_img)
COMPUTE_SOURCE("compositor_glare_ghost_base.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_accumulate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float4, scales)
PUSH_CONSTANT_ARRAY(float4, color_modulators, 4)
SAMPLER(0, sampler2D, input_ghost_tx)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, accumulated_ghost_img)
COMPUTE_SOURCE("compositor_glare_ghost_accumulate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -----------
 * Simple Star
 * ----------- */

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_horizontal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(int, iterations)
PUSH_CONSTANT(float, fade_factor)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, horizontal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_horizontal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_vertical_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(int, iterations)
PUSH_CONSTANT(float, fade_factor)
SAMPLER(0, sampler2D, horizontal_tx)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, vertical_img)
COMPUTE_SOURCE("compositor_glare_simple_star_vertical_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_diagonal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(int, iterations)
PUSH_CONSTANT(float, fade_factor)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, diagonal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_diagonal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_anti_diagonal_pass)
LOCAL_GROUP_SIZE(16)
PUSH_CONSTANT(int, iterations)
PUSH_CONSTANT(float, fade_factor)
SAMPLER(0, sampler2D, diagonal_tx)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, anti_diagonal_img)
COMPUTE_SOURCE("compositor_glare_simple_star_anti_diagonal_pass.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -------
 * Streaks
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_filter)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, color_modulator)
PUSH_CONSTANT(float3, fade_factors)
PUSH_CONSTANT(float2, streak_vector)
SAMPLER(0, sampler2D, input_streak_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_streak_img)
COMPUTE_SOURCE("compositor_glare_streaks_filter.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_accumulate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, attenuation_factor)
SAMPLER(0, sampler2D, streak_tx)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, accumulated_streaks_img)
COMPUTE_SOURCE("compositor_glare_streaks_accumulate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -----
 * Bloom
 * ----- */

GPU_SHADER_CREATE_INFO(compositor_glare_bloom_downsample_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
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
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_bloom_upsample.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -----
 * Sun Beams
 * ----- */

GPU_SHADER_CREATE_INFO(compositor_glare_sun_beams_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, source)
PUSH_CONSTANT(int, max_steps)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_glare_sun_beams.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_sun_beams)
ADDITIONAL_INFO(compositor_glare_sun_beams_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_sun_beams_jitter)
ADDITIONAL_INFO(compositor_glare_sun_beams_shared)
DEFINE("JITTER")
PUSH_CONSTANT(float, jitter_factor)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* ------
 * Kernel
 * ------ */

GPU_SHADER_CREATE_INFO(compositor_glare_kernel_downsample_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_glare_kernel_downsample.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_kernel_downsample_color)
ADDITIONAL_INFO(compositor_glare_kernel_downsample_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_glare_kernel_downsample_float)
ADDITIONAL_INFO(compositor_glare_kernel_downsample_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
