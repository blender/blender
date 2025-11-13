/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_convert_float_shared)
LOCAL_GROUP_SIZE(16, 16)
COMPUTE_SOURCE("compositor_convert.glsl")
SAMPLER(0, sampler2D, input_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_shared)
LOCAL_GROUP_SIZE(16, 16)
COMPUTE_SOURCE("compositor_convert.glsl")
SAMPLER(0, isampler2D, input_tx)
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Int to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_int2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_int_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_bool)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_int_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Int2 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_int)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_bool)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_int2_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float2 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float2_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float3 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float3_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_color_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_color_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_color_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_color_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_color_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_color_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_color_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float4 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_float4_to_bool")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Bool to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_int)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_int")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_int2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_int2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_color")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_bool_to_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to channel.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_alpha)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_FUNCTION("convert_color_to_alpha")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
