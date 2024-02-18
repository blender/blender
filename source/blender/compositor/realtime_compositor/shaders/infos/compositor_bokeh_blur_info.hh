/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_bokeh_blur)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "radius")
    .push_constant(Type::BOOL, "extend_bounds")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "weights_tx")
    .sampler(2, ImageType::FLOAT_2D, "mask_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_bokeh_blur.glsl")
    .do_static_compilation(true);
