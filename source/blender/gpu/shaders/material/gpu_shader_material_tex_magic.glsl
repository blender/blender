/* SPDX-FileCopyrightText: 2019-2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_tex_magic(
    float3 co, float scale, float distortion, float depth, out float4 color, out float fac)
{
  float3 p = mod(co * scale, 2.0f * M_PI);

  float x = sin((p.x + p.y + p.z) * 5.0f);
  float y = cos((-p.x + p.y - p.z) * 5.0f);
  float z = -cos((-p.x - p.y + p.z) * 5.0f);

  if (depth > 0) {
    x *= distortion;
    y *= distortion;
    z *= distortion;
    y = -cos(x - y + z);
    y *= distortion;
    if (depth > 1) {
      x = cos(x - y - z);
      x *= distortion;
      if (depth > 2) {
        z = sin(-x - y - z);
        z *= distortion;
        if (depth > 3) {
          x = -cos(-x + y - z);
          x *= distortion;
          if (depth > 4) {
            y = -sin(-x + y + z);
            y *= distortion;
            if (depth > 5) {
              y = -cos(-x + y + z);
              y *= distortion;
              if (depth > 6) {
                x = cos(x + y + z);
                x *= distortion;
                if (depth > 7) {
                  z = sin(x + y - z);
                  z *= distortion;
                  if (depth > 8) {
                    x = -cos(-x - y + z);
                    x *= distortion;
                    if (depth > 9) {
                      y = -sin(x - y + z);
                      y *= distortion;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (distortion != 0.0f) {
    distortion *= 2.0f;
    x /= distortion;
    y /= distortion;
    z /= distortion;
  }

  color = float4(0.5f - x, 0.5f - y, 0.5f - z, 1.0f);
  fac = (color.x + color.y + color.z) / 3.0f;
}
