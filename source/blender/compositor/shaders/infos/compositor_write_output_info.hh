/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_write_output_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int2, lower_bound)
PUSH_CONSTANT(int2, upper_bound)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_write_output.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_write_output)
ADDITIONAL_INFO(compositor_write_output_shared)
DEFINE("DIRECT_OUTPUT")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_write_output_opaque)
ADDITIONAL_INFO(compositor_write_output_shared)
DEFINE("OPAQUE_OUTPUT")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_write_output_alpha)
ADDITIONAL_INFO(compositor_write_output_shared)
SAMPLER(1, sampler2D, alpha_tx)
DEFINE("ALPHA_OUTPUT")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
