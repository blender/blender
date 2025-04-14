/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_color_spill(float4 color,
                                float factor,
                                const float spill_channel,
                                float3 spill_scale,
                                const float2 limit_channels,
                                float limit_scale,
                                out float4 result)
{
  float average_limit = (color[int(limit_channels.x)] + color[int(limit_channels.y)]) / 2.0f;
  float map = factor * color[int(spill_channel)] - limit_scale * average_limit;
  result = float4(map > 0.0f ? color.rgb + spill_scale * map : color.rgb, color.a);
}
