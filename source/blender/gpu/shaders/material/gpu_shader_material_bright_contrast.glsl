/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void brightness_contrast(float4 col, float brightness, float contrast, out float4 outcol)
{
  float a = 1.0f + contrast;
  float b = brightness - contrast * 0.5f;

  outcol.r = max(a * col.r + b, 0.0f);
  outcol.g = max(a * col.g + b, 0.0f);
  outcol.b = max(a * col.b + b, 0.0f);
  outcol.a = col.a;
}
