/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_read_input_shared)
    .local_group_size(16, 16)
    .push_constant(Type::IVEC2, "lower_bound")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .compute_source("compositor_read_input.glsl");

GPU_SHADER_CREATE_INFO(compositor_read_input_float)
    .additional_info("compositor_read_input_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(input_color)", "vec4(input_color.r, vec3(0.0))")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_read_input_vector)
    .additional_info("compositor_read_input_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(input_color)", "input_color")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_read_input_color)
    .additional_info("compositor_read_input_shared")
    .push_constant(Type::BOOL, "premultiply_alpha")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(input_color)",
            "input_color * vec4(vec3(premultiply_alpha ? input_color.a : 1.0), 1.0)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_read_input_alpha)
    .additional_info("compositor_read_input_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .define("READ_EXPRESSION(input_color)", "vec4(input_color.a, vec3(0.0))")
    .do_static_compilation(true);
