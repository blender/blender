/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_write_output_shared)
    .local_group_size(16, 16)
    .push_constant(Type::IVEC2, "compositing_region_lower_bound")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_write_output.glsl");

GPU_SHADER_CREATE_INFO(compositor_write_output)
    .additional_info("compositor_write_output_shared")
    .define("DIRECT_OUTPUT")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_write_output_opaque)
    .additional_info("compositor_write_output_shared")
    .define("OPAQUE_OUTPUT")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_write_output_alpha)
    .additional_info("compositor_write_output_shared")
    .sampler(1, ImageType::FLOAT_2D, "alpha_tx")
    .define("ALPHA_OUTPUT")
    .do_static_compilation(true);
