/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_fresnel.glsl"

void node_layer_weight(float blend, float3 N, out float fresnel, out float facing)
{
  N = normalize(N);

  /* fresnel */
  float eta = max(1.0f - blend, 0.00001f);
  float3 V = coordinate_incoming(g_data.P);

  fresnel = fresnel_dielectric(V, N, (FrontFacing) ? 1.0f / eta : eta);

  /* facing */
  facing = abs(dot(V, N));
  if (blend != 0.5f) {
    blend = clamp(blend, 0.0f, 0.99999f);
    blend = (blend < 0.5f) ? 2.0f * blend : 0.5f / (1.0f - blend);
    facing = pow(facing, blend);
  }
  facing = 1.0f - facing;
}
