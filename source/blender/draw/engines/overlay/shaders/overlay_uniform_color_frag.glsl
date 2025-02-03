/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  fragColor = ucolor;
#ifdef LINE_OUTPUT
  lineOutput = vec4(0.0);
#endif
}
