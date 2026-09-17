/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_wireframe(float size, const ShadingData &sd, float &fac)
{
  float3 barys = sd.barycentric_coords.xyy;
  barys.z = 1.0f - barys.x - barys.y;

  size *= 0.5f;
  float3 s = step(-size, -barys * sd.barycentric_dists);

  fac = max(s.x, max(s.y, s.z));
}

[[node]]
void node_wireframe_screenspace(float size,
                                [[resource_table]] KernelGlobals &kg,
                                const ShadingData &sd,
                                float &fac)
{
  float3 barys = sd.barycentric_coords.xyy;
  barys.z = 1.0f - barys.x - barys.y;

#if defined(GPU_FRAGMENT_SHADER) || defined(GLSL_CPP_STUBS)
  size *= (1.0f / 3.0f);
  float3 dx = gpu_dfdx(barys) * derivative_scale_get(kg);
  float3 dy = gpu_dfdy(barys) * derivative_scale_get(kg);
  float3 deltas = sqrt(dx * dx + dy * dy);

  float3 s = step(-deltas * size, -barys);

  fac = max(s.x, max(s.y, s.z));
#else
  fac = 1.0f;
#endif
}
