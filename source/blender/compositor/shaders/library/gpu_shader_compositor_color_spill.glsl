/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float3 compute_spill_scale(bool use_spill_strength, float4 spill_strength, int spill_channel)
{
  if (use_spill_strength) {
    float3 scale = spill_strength.xyz;
    scale[spill_channel] *= -1.0f;
    return scale;
  }

  float3 scale = float3(0.0f);
  scale[spill_channel] = -1.0f;
  return scale;
}

void node_composite_color_spill(float4 color,
                                float factor,
                                float limit_strength,
                                float use_spill_strength,
                                float4 spill_strength,
                                const float spill_channel,
                                const float2 limit_channels,
                                out float4 result)
{
  float average_limit = (color[int(limit_channels.x)] + color[int(limit_channels.y)]) / 2.0f;
  float map = factor * color[int(spill_channel)] - limit_strength * average_limit;
  float3 spill_scale = compute_spill_scale(
      use_spill_strength != 0.0f, spill_strength, int(spill_channel));
  result = float4(map > 0.0f ? color.rgb + spill_scale * map : color.rgb, color.a);
}
