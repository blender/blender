/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_output_aov(float4 color, float value, float hash, Closure &dummy)
{
  output_aov(color, value, floatBitsToUint(hash));
}
