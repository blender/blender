/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_translate_wrapped_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, translation)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_translate_wrapped.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_translate_wrapped)
ADDITIONAL_INFO(compositor_translate_wrapped_shared)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_translate_wrapped_bicubic)
ADDITIONAL_INFO(compositor_translate_wrapped_shared)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture_bicubic")
GPU_SHADER_CREATE_END()
