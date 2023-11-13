/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_tone_map_simple)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "luminance_scale")
    .push_constant(Type::FLOAT, "luminance_scale_blend_factor")
    .push_constant(Type::FLOAT, "inverse_gamma")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_tone_map_simple.glsl")
    .do_static_compilation(true);
