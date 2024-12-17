/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(MAT4, inverse_transformation)
SAMPLER(0, FLOAT_2D, input_tx)
COMPUTE_SOURCE("compositor_realize_on_domain.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_standard_shared)
ADDITIONAL_INFO(compositor_realize_on_domain_shared)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_shared)
ADDITIONAL_INFO(compositor_realize_on_domain_shared)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture_bicubic")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_color)
ADDITIONAL_INFO(compositor_realize_on_domain_standard_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_vector)
ADDITIONAL_INFO(compositor_realize_on_domain_standard_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_float)
ADDITIONAL_INFO(compositor_realize_on_domain_standard_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_color)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_vector)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
