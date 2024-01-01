/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_R16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "blurred_input_img")
    .compute_source("compositor_morphological_blur.glsl");

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_dilate)
    .additional_info("compositor_morphological_blur_shared")
    .define("OPERATOR(x, y)", "max(x, y)")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_erode)
    .additional_info("compositor_morphological_blur_shared")
    .define("OPERATOR(x, y)", "min(x, y)")
    .do_static_compilation(true);
