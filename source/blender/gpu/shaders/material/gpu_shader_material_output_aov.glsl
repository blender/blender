/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_output_aov(float4 color, float value, float hash, Closure &dummy)
{
#ifdef GPU_FRAGMENT_SHADER
#  ifdef OBINFO_LIB
  output_aov(int2(gl_FragCoord.xy),
             color,
             value,
             floatBitsToUint(hash),
             g_holdout,
             object_infos_get().flag);
#  else
  output_aov(int2(gl_FragCoord.xy), color, value, floatBitsToUint(hash), 0.0f, 0u);
#  endif
#endif
}
