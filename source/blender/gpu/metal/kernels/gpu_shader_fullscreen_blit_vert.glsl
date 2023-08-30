/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* The position represents a 0-1 square, we first scale it by the size we want to have it on
   * screen next we divide by the full-screen size, this will bring everything in range [0,1].
   * Next we scale to NDC range [-1,1]. */
  gl_Position = vec4((((pos * size + dst_offset) / fullscreen) * 2.0 - 1.0), 1.0, 1.0);
  vec2 uvoff = (src_offset / fullscreen);
  uvcoordsvar = vec4(pos + uvoff, 0.0, 0.0);
}
