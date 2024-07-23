/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  uint px = texture(image, uvcoordsvar.xy).r;
  fragColor = vec4(1.0, 1.0, 1.0, 0.0);
  if (px != 0u) {
    fragColor.a = 1.0;
    px &= 0x3Fu;
    fragColor.r = ((px >> 0) & 0x3u) / float(0x3u);
    fragColor.g = ((px >> 2) & 0x3u) / float(0x3u);
    fragColor.b = ((px >> 4) & 0x3u) / float(0x3u);
  }
}
