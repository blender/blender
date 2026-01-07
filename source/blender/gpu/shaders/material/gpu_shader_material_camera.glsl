/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

[[node]]
void camera(float3 &outview, float &outdepth, float &outdist)
{
  float3 vP;
  point_transform_world_to_view(g_data.P, vP);
  vP.z = -vP.z;
  outdepth = abs(vP.z);
  outdist = length(vP);
  outview = normalize(vP);
}
