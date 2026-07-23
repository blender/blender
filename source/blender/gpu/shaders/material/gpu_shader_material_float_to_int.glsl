/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void float_to_int_round(float value, out int result)
{
  result = int(round(value));
}

[[node]]
void float_to_int_floor(float value, out int result)
{
  result = int(floor(value));
}

[[node]]
void float_to_int_ceil(float value, out int result)
{
  result = int(ceil(value));
}

[[node]]
void float_to_int_truncate(float value, out int result)
{
  result = int(trunc(value));
}
