/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/workbench_prepass_infos.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(workbench_prepass)
#endif

float2 matcap_uv_compute(float3 I, float3 N, bool flipped)
{
  /* Quick creation of an orthonormal basis */
  float a = 1.0f / (1.0f + I.z);
  float b = -I.x * I.y * a;
  float3 b1 = float3(1.0f - I.x * I.x * a, b, -I.x);
  float3 b2 = float3(b, 1.0f - I.y * I.y * a, -I.y);
  float2 matcap_uv = float2(dot(b1, N), dot(b2, N));
  if (flipped) {
    matcap_uv.x = -matcap_uv.x;
  }
  return matcap_uv * 0.496f + 0.5f;
}

float3 get_matcap_lighting(
    sampler2D diffuse_matcap, sampler2D specular_matcap, float3 base_color, float3 N, float3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  float2 uv = matcap_uv_compute(I, N, flipped);

  float3 diffuse = textureLod(diffuse_matcap, uv, 0.0f).rgb;
  float3 specular = textureLod(specular_matcap, uv, 0.0f).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}

float3 get_matcap_lighting(sampler2DArray matcap, float3 base_color, float3 N, float3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  float2 uv = matcap_uv_compute(I, N, flipped);

  float3 diffuse = textureLod(matcap, float3(uv, 0.0f), 0.0f).rgb;
  float3 specular = textureLod(matcap, float3(uv, 1.0f), 0.0f).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}
