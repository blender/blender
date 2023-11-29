/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_material_transform_utils.glsl)

void camera(out vec3 outview, out float outdepth, out float outdist)
{
  vec3 vP;
  point_transform_world_to_view(g_data.P, vP);
  vP.z = -vP.z;
  outdepth = abs(vP.z);
  outdist = length(vP);
  outview = normalize(vP);
}
