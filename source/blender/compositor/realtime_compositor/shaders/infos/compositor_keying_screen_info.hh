/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_keying_screen)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "smoothness")
    .push_constant(Type::INT, "number_of_markers")
    .storage_buf(0, Qualifier::READ, "vec2", "marker_positions[]")
    .storage_buf(1, Qualifier::READ, "vec4", "marker_colors[]")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_keying_screen.glsl")
    .do_static_compilation(true);
