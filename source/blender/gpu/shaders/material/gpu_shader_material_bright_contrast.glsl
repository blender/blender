/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void brightness_contrast(vec4 col, float brightness, float contrast, out vec4 outcol)
{
  float a = 1.0 + contrast;
  float b = brightness - contrast * 0.5;

  outcol.r = max(a * col.r + b, 0.0);
  outcol.g = max(a * col.g + b, 0.0);
  outcol.b = max(a * col.b + b, 0.0);
  outcol.a = col.a;
}
