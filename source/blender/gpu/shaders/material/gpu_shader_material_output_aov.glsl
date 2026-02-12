/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_output_aov(float4 color, float value, float hash, Closure &dummy)
{
#ifdef OBINFO_LIB
  output_aov(color, value, floatBitsToUint(hash), g_holdout, drw_object_infos().flag);
#else
  output_aov(color, value, floatBitsToUint(hash), 0.0f, 0u);
#endif
}
