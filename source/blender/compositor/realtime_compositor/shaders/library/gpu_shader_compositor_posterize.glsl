/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_posterize(vec4 color, float steps, out vec4 result)
{
  steps = clamp(steps, 2.0, 1024.0);
  result = floor(color * steps) / steps;
  result.a = color.a;
}
