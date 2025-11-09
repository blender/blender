/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------
 * Common.
 * ------- */

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float4x4, inverse_transformation)
COMPUTE_SOURCE("compositor_realize_on_domain.glsl")
GPU_SHADER_CREATE_END()

/* -----------------------
 * Float Nearest/Bilinear.
 * ----------------------- */

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_nearest_bilinear_float_shared)
ADDITIONAL_INFO(compositor_realize_on_domain_shared)
SAMPLER(0, Float2D, input_tx)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_float)
ADDITIONAL_INFO(compositor_realize_on_domain_nearest_bilinear_float_shared)
IMAGE(0, SFLOAT_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_float2)
ADDITIONAL_INFO(compositor_realize_on_domain_nearest_bilinear_float_shared)
IMAGE(0, SFLOAT_16_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_float4)
ADDITIONAL_INFO(compositor_realize_on_domain_nearest_bilinear_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------
 * Float Bicubic.
 * -------------- */

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float_shared)
ADDITIONAL_INFO(compositor_realize_on_domain_shared)
SAMPLER(0, Float2D, input_tx)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture_bicubic")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_float_shared)
IMAGE(0, SFLOAT_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float2)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_float_shared)
IMAGE(0, SFLOAT_16_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float4)
ADDITIONAL_INFO(compositor_realize_on_domain_bicubic_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, Float2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* ----
 * Int.
 * ---- */

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_int_shared)
ADDITIONAL_INFO(compositor_realize_on_domain_shared)
SAMPLER(0, Int2D, input_tx)
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_int)
ADDITIONAL_INFO(compositor_realize_on_domain_int_shared)
IMAGE(0, SINT_16, write, Int2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_int2)
ADDITIONAL_INFO(compositor_realize_on_domain_int_shared)
IMAGE(0, SINT_16_16, write, Int2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_bool)
ADDITIONAL_INFO(compositor_realize_on_domain_int_shared)
IMAGE(0, SINT_8, write, Int2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_realize_on_domain_menu)
ADDITIONAL_INFO(compositor_realize_on_domain_int_shared)
IMAGE(0, SINT_8, write, Int2D, domain_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
