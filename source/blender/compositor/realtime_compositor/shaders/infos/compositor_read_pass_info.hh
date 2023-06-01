/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_read_pass_shared)
    .local_group_size(16, 16)
    .push_constant(Type::IVEC2, "compositing_region_lower_bound")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .compute_source("compositor_read_pass.glsl");

GPU_SHADER_CREATE_INFO(compositor_read_pass)
    .additional_info("compositor_read_pass_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(pass_color)", "pass_color")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_read_pass_alpha)
    .additional_info("compositor_read_pass_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(pass_color)", "vec4(pass_color.a, vec3(0.0))")
    .do_static_compilation(true);
