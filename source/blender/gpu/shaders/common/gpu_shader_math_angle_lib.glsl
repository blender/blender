/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "gpu_shader_math_constants_lib.glsl"

struct AngleRadian {
  /* Note that value is public because of the lack of casting operator in GLSL. */
  float angle;

  static AngleRadian identity()
  {
    return {0};
  }

  static AngleRadian from_degree(float angle_degree)
  {
    return {angle_degree * (M_PI / 180.0f)};
  }
};
