/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void rotation_to_quaternion(float4 rotation, float &w, float &x, float &y, float &z)
{
  w = rotation.x;
  x = rotation.y;
  y = rotation.z;
  z = rotation.w;
}
