/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* Should be 0.8 but minimize the AA on the edges. */
  float dist = (length(uv) - 0.78) * scale;

  fragColor = color;
  fragColor.a *= smoothstep(-0.09, 1.09, dist);
}
