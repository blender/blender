/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_output_aov(vec4 color, float value, float hash, out Closure dummy)
{
  output_aov(color, value, floatBitsToUint(hash));
}
