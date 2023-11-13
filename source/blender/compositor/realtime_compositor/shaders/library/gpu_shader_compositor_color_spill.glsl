/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_color_spill(vec4 color,
                                float factor,
                                const float spill_channel,
                                vec3 spill_scale,
                                const vec2 limit_channels,
                                float limit_scale,
                                out vec4 result)
{
  float average_limit = (color[int(limit_channels.x)] + color[int(limit_channels.y)]) / 2.0;
  float map = factor * color[int(spill_channel)] - limit_scale * average_limit;
  result.rgb = map > 0.0 ? color.rgb + spill_scale * map : color.rgb;
  result.a = color.a;
}
