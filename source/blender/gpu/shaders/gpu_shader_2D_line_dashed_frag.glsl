/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Fragment Shader for dashed lines, with uniform multi-color(s),
 * or any single-color, and any thickness.
 *
 * Dashed is performed in screen space.
 */

void main()
{
  float distance_along_line = distance(stipple_pos, stipple_start);
  /* Solid line case, simple. */
  if (udash_factor >= 1.0f) {
    fragColor = color;
  }
  /* Actually dashed line... */
  else {
    float normalized_distance = fract(distance_along_line / dash_width);
    if (normalized_distance <= udash_factor) {
      fragColor = color;
    }
    else if (colors_len > 0) {
      fragColor = color2;
    }
    else {
      discard;
    }
  }
}
