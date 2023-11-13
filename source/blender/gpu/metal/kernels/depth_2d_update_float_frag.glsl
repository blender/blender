/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  gl_FragDepth = textureLod(source_data, texCoord_interp, mip).r;
}
