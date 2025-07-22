/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_pad_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int2, size)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_pad.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_pad_zero_float4)
ADDITIONAL_INFO(compositor_pad_shared)
COMPILATION_CONSTANT(bool, zero_pad, true)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_pad_extend_float)
ADDITIONAL_INFO(compositor_pad_shared)
COMPILATION_CONSTANT(bool, zero_pad, false)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_pad_extend_float2)
ADDITIONAL_INFO(compositor_pad_shared)
COMPILATION_CONSTANT(bool, zero_pad, false)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
