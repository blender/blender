/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_shared)
    .local_group_size(16, 16)
    .push_constant(Type::MAT4, "inverse_transformation")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .compute_source("compositor_realize_on_domain.glsl");

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_standard_shared)
    .additional_info("compositor_realize_on_domain_shared")
    .define("SAMPLER_FUNCTION", "texture");

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_shared)
    .additional_info("compositor_realize_on_domain_shared")
    .define("SAMPLER_FUNCTION", "texture_bicubic");

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_color)
    .additional_info("compositor_realize_on_domain_standard_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_vector)
    .additional_info("compositor_realize_on_domain_standard_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_float)
    .additional_info("compositor_realize_on_domain_standard_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_color)
    .additional_info("compositor_realize_on_domain_bicubic_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_vector)
    .additional_info("compositor_realize_on_domain_bicubic_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float)
    .additional_info("compositor_realize_on_domain_bicubic_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "domain_img")
    .do_static_compilation(true);
