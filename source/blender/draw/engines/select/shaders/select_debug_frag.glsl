/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  uint px = texture(image, screen_uv).r;
  frag_color = float4(1.0f, 1.0f, 1.0f, 0.0f);
  if (px != 0u) {
    frag_color.a = 1.0f;
    px &= 0x3Fu;
    frag_color.r = ((px >> 0) & 0x3u) / float(0x3u);
    frag_color.g = ((px >> 2) & 0x3u) / float(0x3u);
    frag_color.b = ((px >> 4) & 0x3u) / float(0x3u);
  }
}
