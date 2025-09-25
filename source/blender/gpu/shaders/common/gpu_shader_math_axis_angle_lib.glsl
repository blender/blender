/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct AxisAngle {
  float3 axis;
  float angle;

  METAL_CONSTRUCTOR_2(AxisAngle, float3, axis, float, angle)

  static AxisAngle identity()
  {
    return AxisAngle(float3(0, 1, 0), 0);
  }
};
