/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_quaternion.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

[[node]]
void invert_rotation(float4 rotation, float4 &result)
{
  result = quaternion_conjugate(Quaternion{UNPACK4(rotation)}).as_float4();
}
