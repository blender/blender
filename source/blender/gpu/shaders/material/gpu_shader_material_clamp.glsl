/* SPDX-FileCopyrightText: 2019-2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void clamp_value(float value, float min, float max, float &result)
{
  result = clamp(value, min, max);
}

[[node]]
void clamp_minmax(float value, float min_allowed, float max_allowed, float &result)
{
  result = min(max(value, min_allowed), max_allowed);
}

[[node]]
void clamp_range(float value, float min, float max, float &result)
{
  result = (max > min) ? clamp(value, min, max) : clamp(value, max, min);
}
