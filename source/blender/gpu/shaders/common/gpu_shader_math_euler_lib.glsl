/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct EulerXYZ {
  float x, y, z;

  static EulerXYZ from_float3(float3 eul)
  {
    return {eul.x, eul.y, eul.z};
  }

  static EulerXYZ identity()
  {
    return {0, 0, 0};
  }

  float3 as_float3() const
  {
    return float3(this->x, this->y, this->z);
  }
};
