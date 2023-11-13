/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  out_color = vec4(vec3(0.0), 1.0 - mask_weight);
}
