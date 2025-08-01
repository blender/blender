/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_tone_map_photoreceptor)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float4, global_adaptation_level)
PUSH_CONSTANT(float, contrast)
PUSH_CONSTANT(float, intensity)
PUSH_CONSTANT(float, chromatic_adaptation)
PUSH_CONSTANT(float, light_adaptation)
PUSH_CONSTANT(float3, luminance_coefficients)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_tone_map_photoreceptor.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
