/* SPDX-License-Identifier: GPL-2.0-or-later */

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
