/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float depth = texture(depth_tx, uvcoordsvar.xy).r;
  if (depth != 1.0) {
    gl_FragDepth = depth;
  }
  else {
    discard;
  }
}
