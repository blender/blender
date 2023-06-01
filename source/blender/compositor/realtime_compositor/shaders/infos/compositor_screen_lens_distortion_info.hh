/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_screen_lens_distortion_shared)
    .local_group_size(16, 16)
    .push_constant(Type::VEC3, "chromatic_distortion")
    .push_constant(Type::FLOAT, "scale")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_screen_lens_distortion.glsl");

GPU_SHADER_CREATE_INFO(compositor_screen_lens_distortion)
    .additional_info("compositor_screen_lens_distortion_shared")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_screen_lens_distortion_jitter)
    .additional_info("compositor_screen_lens_distortion_shared")
    .define("JITTER")
    .do_static_compilation(true);
