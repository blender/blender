/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void separate_xyz(float3 vec, float &x, float &y, float &z)
{
  x = vec.r;
  y = vec.g;
  z = vec.b;
}
