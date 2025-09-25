/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_displace_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, displacement_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_displace)
ADDITIONAL_INFO(compositor_displace_shared)
COMPUTE_SOURCE("compositor_displace.glsl")
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_displace_bicubic)
ADDITIONAL_INFO(compositor_displace_shared)
COMPUTE_SOURCE("compositor_displace.glsl")
DEFINE_VALUE("SAMPLER_FUNCTION", "texture_bicubic")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_displace_anisotropic)
ADDITIONAL_INFO(compositor_displace_shared)
COMPUTE_SOURCE("compositor_displace_anisotropic.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
