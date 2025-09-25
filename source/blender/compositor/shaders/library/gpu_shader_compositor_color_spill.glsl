/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE 0
#define CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE 1

/* Compute the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Average, if it is a single limit channel, store it in both limit channels, because
 * the average of two identical values is the same value. */
int2 compute_limit_channels(const int limit_method,
                            const int spill_channel,
                            const int limit_channel)
{
  /* If the algorithm is Average, store the indices of the other two channels other than the spill
   * channel. */
  if (limit_method == CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE) {
    return int2((spill_channel + 1) % 3, (spill_channel + 2) % 3);
  }

  /* If the algorithm is Single, store the index of the limit channel in both channels. */
  return int2(limit_channel);
}

float3 compute_spill_scale(const bool use_spill_strength,
                           const float4 spill_strength,
                           const int spill_channel)
{
  if (use_spill_strength) {
    float3 scale = spill_strength.xyz();
    scale[spill_channel] *= -1.0f;
    return scale;
  }

  float3 scale = float3(0.0f);
  scale[spill_channel] = -1.0f;
  return scale;
}

void node_composite_color_spill(float4 color,
                                float factor,
                                float spill_channel,
                                float limit_method,
                                float limit_channel,
                                float limit_strength,
                                float use_spill_strength,
                                float4 spill_strength,
                                out float4 result)
{
  const int2 limit_channels = compute_limit_channels(
      int(limit_method), int(spill_channel), int(limit_channel));
  const float average_limit = (color[limit_channels.x] + color[limit_channels.y]) / 2.0f;
  const float map = factor * color[int(spill_channel)] - limit_strength * average_limit;
  const float3 spill_scale = compute_spill_scale(
      use_spill_strength != 0.0f, spill_strength, int(spill_channel));
  result = float4(map > 0.0f ? color.rgb + spill_scale * map : color.rgb, color.a);
}

#undef CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE
#undef CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE
