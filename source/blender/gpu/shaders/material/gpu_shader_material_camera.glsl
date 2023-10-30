/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void camera(out vec3 outview, out float outdepth, out float outdist)
{
  vec3 vP = transform_point(ViewMatrix, g_data.P);
  vP.z = -vP.z;
  outdepth = abs(vP.z);
  outdist = length(vP);
  outview = normalize(vP);
}
