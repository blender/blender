/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_convert_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
TYPEDEF_SOURCE("gpu_shader_compositor_type_conversion.glsl")
COMPUTE_SOURCE("compositor_convert.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_float)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_vector)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(vec3_from_float(value.x), 1.0)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_float_to_color)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4_from_float(value.x)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_float)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_from_vec4(value), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_vector)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_color)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_vector_to_float)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(float_from_vec3(value.xyz), vec3(0.0))")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_vector_to_vector)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_vector_to_color)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4_from_vec3(value.xyz)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_convert_color_to_alpha)
ADDITIONAL_INFO(compositor_convert_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DEFINE_VALUE("CONVERT_EXPRESSION(value)", "vec4(value.a)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
