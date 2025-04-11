/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* An arbitrary value determined by Blender. */
#define BLENDER_ZMAX 10000.0f

void node_composite_map_range(float value,
                              float from_min,
                              float from_max,
                              float to_min,
                              float to_max,
                              const float should_clamp,
                              out float result)
{
  if (abs(from_max - from_min) < 1e-6f) {
    result = 0.0f;
  }
  else {
    if (value >= -BLENDER_ZMAX && value <= BLENDER_ZMAX) {
      result = (value - from_min) / (from_max - from_min);
      result = to_min + result * (to_max - to_min);
    }
    else if (value > BLENDER_ZMAX) {
      result = to_max;
    }
    else {
      result = to_min;
    }

    if (should_clamp != 0.0f) {
      if (to_max > to_min) {
        result = clamp(result, to_min, to_max);
      }
      else {
        result = clamp(result, to_max, to_min);
      }
    }
  }
}

void node_composite_map_value(float value,
                              float offset,
                              float size,
                              const float use_min,
                              float min,
                              const float use_max,
                              float max,
                              out float result)
{
  result = (value + offset) * size;

  if (use_min != 0.0f && result < min) {
    result = min;
  }

  if (use_max != 0.0f && result > max) {
    result = max;
  }
}
