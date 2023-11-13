/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void separate_xyz(vec3 vec, out float x, out float y, out float z)
{
  x = vec.r;
  y = vec.g;
  z = vec.b;
}
