/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_write_output_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(IVEC2, lower_bound)
PUSH_CONSTANT(IVEC2, upper_bound)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
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
SAMPLER(1, FLOAT_2D, alpha_tx)
DEFINE("ALPHA_OUTPUT")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
