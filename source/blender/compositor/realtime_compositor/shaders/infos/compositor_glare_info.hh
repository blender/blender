/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------
 * Common.
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_highlights)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "threshold")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_glare_highlights.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_mix)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "mix_factor")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "glare_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_glare_mix.glsl")
    .do_static_compilation(true);

/* ------------
 * Ghost Glare.
 * ------------ */

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_base)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "small_ghost_tx")
    .sampler(1, ImageType::FLOAT_2D, "big_ghost_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "combined_ghost_img")
    .compute_source("compositor_glare_ghost_base.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_ghost_accumulate)
    .local_group_size(16, 16)
    .push_constant(Type::VEC4, "scales")
    .push_constant(Type::VEC4, "color_modulators", 4)
    .sampler(0, ImageType::FLOAT_2D, "input_ghost_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "accumulated_ghost_img")
    .compute_source("compositor_glare_ghost_accumulate.glsl")
    .do_static_compilation(true);

/* -----------
 * Simple Star
 * ----------- */

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_horizontal_pass)
    .local_group_size(16)
    .push_constant(Type::INT, "iterations")
    .push_constant(Type::FLOAT, "fade_factor")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "horizontal_img")
    .compute_source("compositor_glare_simple_star_horizontal_pass.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_vertical_pass)
    .local_group_size(16)
    .push_constant(Type::INT, "iterations")
    .push_constant(Type::FLOAT, "fade_factor")
    .sampler(0, ImageType::FLOAT_2D, "horizontal_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "vertical_img")
    .compute_source("compositor_glare_simple_star_vertical_pass.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_diagonal_pass)
    .local_group_size(16)
    .push_constant(Type::INT, "iterations")
    .push_constant(Type::FLOAT, "fade_factor")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "diagonal_img")
    .compute_source("compositor_glare_simple_star_diagonal_pass.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_simple_star_anti_diagonal_pass)
    .local_group_size(16)
    .push_constant(Type::INT, "iterations")
    .push_constant(Type::FLOAT, "fade_factor")
    .sampler(0, ImageType::FLOAT_2D, "diagonal_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "anti_diagonal_img")
    .compute_source("compositor_glare_simple_star_anti_diagonal_pass.glsl")
    .do_static_compilation(true);

/* -------
 * Streaks
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_filter)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "color_modulator")
    .push_constant(Type::VEC3, "fade_factors")
    .push_constant(Type::VEC2, "streak_vector")
    .sampler(0, ImageType::FLOAT_2D, "input_streak_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_streak_img")
    .compute_source("compositor_glare_streaks_filter.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_streaks_accumulate)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "attenuation_factor")
    .sampler(0, ImageType::FLOAT_2D, "streak_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "accumulated_streaks_img")
    .compute_source("compositor_glare_streaks_accumulate.glsl")
    .do_static_compilation(true);

/* --------
 * Fog Glow
 * -------- */

GPU_SHADER_CREATE_INFO(compositor_glare_fog_glow_downsample_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_glare_fog_glow_downsample.glsl");

GPU_SHADER_CREATE_INFO(compositor_glare_fog_glow_downsample_simple_average)
    .define("SIMPLE_AVERAGE")
    .additional_info("compositor_glare_fog_glow_downsample_shared")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_fog_glow_downsample_karis_average)
    .define("KARIS_AVERAGE")
    .additional_info("compositor_glare_fog_glow_downsample_shared")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_glare_fog_glow_upsample)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_glare_fog_glow_upsample.glsl")
    .do_static_compilation(true);
