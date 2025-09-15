/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct EulerXYZ {
  float x, y, z;
  METAL_CONSTRUCTOR_3(EulerXYZ, float, x, float, y, float, z)

  static EulerXYZ from_float3(float3 eul)
  {
    return EulerXYZ(eul.x, eul.y, eul.z);
  }

  static EulerXYZ identity()
  {
    return EulerXYZ(0, 0, 0);
  }

  float3 as_float3() const
  {
    return float3(this->x, this->y, this->z);
  }
};
