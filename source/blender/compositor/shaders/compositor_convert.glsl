/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_convert_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_convert_float2_to_color)

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_compositor_type_conversion.glsl"

void convert_float_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float_to_int(value.x), int3(0)));
}

void convert_float_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float_to_int2(value.x), int2(0)));
}

void convert_float_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float_to_float2(value.x), float2(0.0f)));
}

void convert_float_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float_to_float3(value.x), 0.0f));
}

void convert_float_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float_to_color(value.x)));
}

void convert_float_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float_to_float4(value.x)));
}

void convert_float_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_float_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_float_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float_to_bool(value.x)));
}

void convert_float2_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float2_to_float(value.xy), float3(0.0f)));
}

void convert_float2_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float2_to_int(value.xy), int3(0)));
}

void convert_float2_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float2_to_int2(value.xy), int2(0)));
}

void convert_float2_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float2_to_float3(value.xy), 0.0f));
}

void convert_float2_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float2_to_color(value.xy)));
}

void convert_float2_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float2_to_float4(value.xy)));
}

void convert_float2_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_float2_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_float2_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float2_to_bool(value.xy)));
}

void convert_float3_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float3_to_float(value.xyz), float3(0.0f)));
}

void convert_float3_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float3_to_int(value.xyz), int3(0)));
}

void convert_float3_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float3_to_int2(value.xyz), int2(0)));
}

void convert_float3_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float3_to_float2(value.xyz), float2(0.0f)));
}

void convert_float3_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float3_to_color(value.xyz)));
}

void convert_float3_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float3_to_float4(value.xyz)));
}

void convert_float3_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_float3_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_float3_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float3_to_bool(value.xyz)));
}

void convert_float4_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float4_to_float(value), float3(0.0f)));
}

void convert_float4_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float4_to_int(value), int3(0)));
}

void convert_float4_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float4_to_int2(value), int2(0)));
}

void convert_float4_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float4_to_float2(value), float2(0.0f)));
}

void convert_float4_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float4_to_float3(value), 0.0f));
}

void convert_float4_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(float4_to_color(value)));
}

void convert_float4_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_float4_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_float4_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(float4_to_bool(value)));
}

void convert_color_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_float, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_float, output_img);
  auto &luma_coefs = push_constant_get(compositor_convert_color_to_float,
                                       luminance_coefficients_u);
  imageStore(image_out, texel, float4(color_to_float(value, luma_coefs)));
}

void convert_color_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_int, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_int, output_img);
  auto &luma_coefs = push_constant_get(compositor_convert_color_to_int, luminance_coefficients_u);
  imageStore(image_out, texel, int4(color_to_int(value, luma_coefs)));
}

void convert_color_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_int2, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_int2, output_img);
  imageStore(image_out, texel, int4(color_to_int2(value), int2(0)));
}

void convert_color_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_float2, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_float2, output_img);
  imageStore(image_out, texel, float4(color_to_float2(value), float2(0.0f)));
}

void convert_color_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_float3, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_float3, output_img);
  imageStore(image_out, texel, float4(color_to_float3(value), 0.0f));
}

void convert_color_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_float4, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_float4, output_img);
  imageStore(image_out, texel, float4(color_to_float4(value)));
}

void convert_color_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_bool, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_bool, output_img);
  auto &luma_coefs = push_constant_get(compositor_convert_color_to_bool, luminance_coefficients_u);
  imageStore(image_out, texel, int4(color_to_bool(value, luma_coefs)));
}

void convert_color_to_alpha()
{
  auto &sampler_in = sampler_get(compositor_convert_color_to_alpha, input_tx);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float4 value = texture_load(sampler_in, texel);
  auto &image_out = image_get(compositor_convert_color_to_alpha, output_img);
  imageStore(image_out, texel, float4(value.a));
}

void convert_int_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(int_to_int2(value.x), int2(0)));
}

void convert_int_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int_to_float(value.x), float3(0.0f)));
}

void convert_int_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int_to_float2(value.x), float2(0.0f)));
}

void convert_int_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int_to_float3(value.x), 0.0f));
}

void convert_int_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int_to_color(value.x)));
}

void convert_int_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int_to_float4(value.x)));
}

void convert_int_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_int_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_int_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(int_to_bool(value.x)));
}

void convert_int2_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(int2_to_int(value.xy), int3(0)));
}

void convert_int2_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int2_to_float(value.xy), float3(0.0f)));
}

void convert_int2_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int2_to_float2(value.xy), float2(0.0f)));
}

void convert_int2_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int2_to_float3(value.xy), 0.0f));
}

void convert_int2_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int2_to_color(value.xy)));
}

void convert_int2_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(int2_to_float4(value.xy)));
}

void convert_int2_to_bool()
{
  auto &sampler_in = sampler_get(compositor_convert_int2_to_bool, input_tx);
  auto &image_out = image_get(compositor_convert_int2_to_bool, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(int2_to_bool(value.xy)));
}

void convert_bool_to_float()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_float, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_float, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(bool_to_float(bool(value.x)), float3(0.0f)));
}

void convert_bool_to_int()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_int, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_int, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(bool_to_int(bool(value.x)), int3(0)));
}

void convert_bool_to_int2()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_int2, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_int2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, int4(bool_to_int2(bool(value.x)), int2(0)));
}

void convert_bool_to_float2()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_float2, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_float2, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(bool_to_float2(bool(value.x)), float2(0.0f)));
}

void convert_bool_to_float3()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_float3, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_float3, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(bool_to_float3(bool(value.x)), 0.0f));
}

void convert_bool_to_color()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_color, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_color, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(bool_to_color(bool(value.x))));
}

void convert_bool_to_float4()
{
  auto &sampler_in = sampler_get(compositor_convert_bool_to_float4, input_tx);
  auto &image_out = image_get(compositor_convert_bool_to_float4, output_img);
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int4 value = texture_load(sampler_in, texel);
  imageStore(image_out, texel, float4(bool_to_float4(bool(value.x))));
}
