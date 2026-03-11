/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_realize_on_domain_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_realize_on_domain_bicubic_float)

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

void realize_on_domain()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);
  const float2 coordinates = transform_point(to_float3x3(transformation), float2(texel));
  imageStore(domain_img, texel, texture(input_tx, coordinates));
}

void realize_on_domain_float4x4()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);
  const float2 coordinates = transform_point(to_float3x3(transformation), float2(texel));
  /* Each column of the matrix is stored in one layer of the texture. */
  for (int i = 0; i < 4; i++) {
    imageStore(domain_img, int3(texel, i), texture(input_tx, float3(coordinates, float(i))));
  }
}

void realize_on_domain_bicubic()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);
  const float2 coordinates = transform_point(to_float3x3(transformation), float2(texel));
  imageStore(domain_img, texel, texture_bicubic(input_tx, coordinates));
}
