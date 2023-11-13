/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  uint val = textureLod(source_data, texCoord_interp, mip).r;
  uint stencil = (val >> 24) & 0xFFu;
  uint depth = (val)&0xFFFFFFu;
  gl_FragDepth = float(depth) / float(0xFFFFFFu);
}
