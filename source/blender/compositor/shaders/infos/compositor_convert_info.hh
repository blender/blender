/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_convert_shared)
LOCAL_GROUP_SIZE(16, 16)
TYPEDEF_SOURCE("gpu_shader_compositor_type_conversion.glsl")
COMPUTE_SOURCE("compositor_convert.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_shared)
ADDITIONAL_INFO(compositor_convert_shared)
SAMPLER(0, sampler2D, input_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_shared)
ADDITIONAL_INFO(compositor_convert_shared)
SAMPLER(0, isampler2D, input_tx)
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float_to_int(value.x), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float_to_int2(value.x), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_float2(value.x), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_float3(value.x), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_color(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_float4(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float_to_bool(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Int to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_int2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(int_to_int2(value.x), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float(value.x), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float2(value.x), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float3(value.x), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_color(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float4(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_bool)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(int_to_bool(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Int2 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_int)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(int2_to_int(value.xy), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int2_to_float(value.xy), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int2_to_float2(value.xy), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int2_to_float3(value.xy), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int2_to_color(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int2_to_float4(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int2_to_bool)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(int2_to_bool(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float2 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float2_to_float(value.xy), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float2_to_int(value.xy), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float2_to_int2(value.xy), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float2_to_float3(value.xy), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float2_to_color(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float2_to_float4(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float2_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float2_to_bool(value.xy))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float3 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_float(value.xyz), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float3_to_int(value.xyz), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float3_to_int2(value.xyz), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_float2(value.xyz), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_color(value.xyz))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_float4(value.xyz))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float3_to_bool(value.xyz))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)",
             "vec4(color_to_float(value, luminance_coefficients_u), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)",
             "ivec4(color_to_int(value, luminance_coefficients_u), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(color_to_int2(value), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(color_to_float2(value), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(color_to_float3(value), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(color_to_float4(value))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(float3, luminance_coefficients_u)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(color_to_bool(value, luminance_coefficients_u))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float4 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_float(value), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float4_to_int(value), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_int2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float4_to_int2(value), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float2)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_float2(value), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_float3(value), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_color(value))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_bool)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SINT_8, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float4_to_bool(value))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Bool to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(bool_to_float(bool(value.x)), vec3(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_int)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(bool_to_int(bool(value.x)), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_int2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SINT_16_16, write, iimage2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(bool_to_int2(bool(value.x)), ivec2(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float2)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(bool_to_float2(bool(value.x)), vec2(0.0f))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(bool_to_float3(bool(value.x)), 0.0f)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(bool_to_color(bool(value.x)))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_bool_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(bool_to_float4(bool(value.x)))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to channel.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_alpha)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(value.a)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
