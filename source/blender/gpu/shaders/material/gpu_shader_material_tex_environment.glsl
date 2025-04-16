/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"

void node_tex_environment_equirectangular(float3 co, out float3 uv)
{
  float3 nco = normalize(co);
  uv.x = -atan(nco.y, nco.x) / (2.0f * M_PI) + 0.5f;
  uv.y = atan(nco.z, hypot(nco.x, nco.y)) / M_PI + 0.5f;
}

void node_tex_environment_mirror_ball(float3 co, out float3 uv)
{
  float3 nco = normalize(co);
  nco.y -= 1.0f;

  float div = 2.0f * sqrt(max(-0.5f * nco.y, 0.0f));
  nco /= max(1e-8f, div);

  uv = 0.5f * nco.xzz + 0.5f;
}

void node_tex_environment_empty(float3 co, out float4 color)
{
  color = float4(1.0f, 0.0f, 1.0f, 1.0f);
}
