/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  uint val = textureLod(source_data, texCoord_interp, mip).r;
  uint depth = (val) & (0xFFFFFFFFu);
  gl_FragDepth = float(depth) / float(0xFFFFFFFFu);
}
