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
SAMPLER(0, FLOAT_2D, input_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_shared)
ADDITIONAL_INFO(compositor_convert_shared)
SAMPLER(0, INT_2D, input_tx)
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16I, WRITE, INT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float_to_int(value.x), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_float3(value.x), 0.0)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_color(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_to_float4(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Int to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float(value.x), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float3)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float3(value.x), 0.0)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_color)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_color(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_int_to_float4)
ADDITIONAL_INFO(compositor_convert_int_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(int_to_float4(value.x))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float3 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_float(value.xyz), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16I, WRITE, INT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float3_to_int(value.xyz), ivec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_color(value.xyz))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float3_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float3_to_float4(value.xyz))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(VEC3, luminance_coefficients_u)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)",
             "vec4(color_to_float(value, luminance_coefficients_u), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
PUSH_CONSTANT(VEC3, luminance_coefficients_u)
IMAGE(0, GPU_R16I, WRITE, INT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)",
             "ivec4(color_to_int(value, luminance_coefficients_u), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(color_to_float3(value), 0.0)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float4)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(color_to_float4(value))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Float4 to other.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_float(value), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_int)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16I, WRITE, INT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "ivec4(float4_to_int(value), ivec3(0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_float3)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_float3(value), 0.0)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float4_to_color)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float4_to_color(value))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* --------------------------------------------------------------------
 * Color to channel.
 */

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_alpha)
ADDITIONAL_INFO(compositor_convert_float_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(value.a)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
