/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct Quaternion {
  float x, y, z, w;

  METAL_CONSTRUCTOR_4(Quaternion, float, x, float, y, float, z, float, w)

  static Quaternion identity()
  {
    return Quaternion(1, 0, 0, 0);
  }

  float4 as_float4() const
  {
    return float4(this->x, this->y, this->z, this->w);
  }
};
