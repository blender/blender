/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* The position represents a 0-1 square, we first scale it by the size we want to have it on
   * screen next we divide by the full-screen size, this will bring everything in range [0,1].
   * Next we scale to NDC range [-1,1]. */
  gl_Position = float4((((pos * size + dst_offset) / fullscreen) * 2.0f - 1.0f), 1.0f, 1.0f);
  float2 uvoff = (src_offset / fullscreen);
  screen_uv = float2(pos + uvoff);
}
