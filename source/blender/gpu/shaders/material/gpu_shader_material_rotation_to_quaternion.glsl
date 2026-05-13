/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void rotation_to_quaternion(float4 rotation, out float w, out float x, out float y, out float z)
{
  w = rotation.x;
  x = rotation.y;
  y = rotation.z;
  z = rotation.w;
}
