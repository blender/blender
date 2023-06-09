/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_box_mask_shared)
    .local_group_size(16, 16)
    .push_constant(Type::IVEC2, "domain_size")
    .push_constant(Type::VEC2, "location")
    .push_constant(Type::VEC2, "size")
    .push_constant(Type::FLOAT, "cos_angle")
    .push_constant(Type::FLOAT, "sin_angle")
    .sampler(0, ImageType::FLOAT_2D, "base_mask_tx")
    .sampler(1, ImageType::FLOAT_2D, "mask_value_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_mask_img")
    .compute_source("compositor_box_mask.glsl");

GPU_SHADER_CREATE_INFO(compositor_box_mask_add)
    .additional_info("compositor_box_mask_shared")
    .define("CMP_NODE_MASKTYPE_ADD")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_box_mask_subtract)
    .additional_info("compositor_box_mask_shared")
    .define("CMP_NODE_MASKTYPE_SUBTRACT")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_box_mask_multiply)
    .additional_info("compositor_box_mask_shared")
    .define("CMP_NODE_MASKTYPE_MULTIPLY")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_box_mask_not)
    .additional_info("compositor_box_mask_shared")
    .define("CMP_NODE_MASKTYPE_NOT")
    .do_static_compilation(true);
