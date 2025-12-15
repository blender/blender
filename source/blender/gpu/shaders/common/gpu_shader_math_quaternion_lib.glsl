/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct Quaternion {
  float x, y, z, w;

  static Quaternion identity()
  {
    return {1, 0, 0, 0};
  }

  float4 as_float4() const
  {
    return float4(this->x, this->y, this->z, this->w);
  }
};
