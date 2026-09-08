/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void float_to_int_round(float value, int &result)
{
  result = int(round(value));
}

[[node]]
void float_to_int_floor(float value, int &result)
{
  result = int(floor(value));
}

[[node]]
void float_to_int_ceil(float value, int &result)
{
  result = int(ceil(value));
}

[[node]]
void float_to_int_truncate(float value, int &result)
{
  result = int(trunc(value));
}
