/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_effect_antialiasing_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_taa)

void main()
{
  float2 texel_size = 1.0f / float2(textureSize(color_buffer, 0));
  float2 uv = gl_FragCoord.xy * texel_size;

  frag_color = float4(0.0f);
  int i = 0;
  [[gpu::unroll]] for (int x = -1; x <= 1; x++)
  {
    [[gpu::unroll]] for (int y = -1; y <= 1; y++, i++)
    {
      float4 color = texture(color_buffer, uv + float2(x, y) * texel_size);
      /* Clamp infinite inputs (See #112211). */
      color = clamp(color, float4(0.0f), float4(1e10f));
      /* Use log2 space to avoid highlights creating too much aliasing. */
      color.rgb = log2(color.rgb + 1.0f);

      frag_color += color * samplesWeights[i];
    }
  }
}
