/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_light_accumulation(const float light_index,
                             float4 diffuse_light,
                             float4 diffuse_color,
                             float4 glossy_light,
                             float4 glossy_color,
                             float4 transmission_light,
                             float4 transmission_color,
                             float weight,
                             Closure &result)
{
  diffuse_light = max(diffuse_light, float4(0.0f));
  diffuse_color = saturate(diffuse_color);
  glossy_light = max(glossy_light, float4(0.0f));
  glossy_color = saturate(glossy_color);
  transmission_light = max(transmission_light, float4(0.0f));
  transmission_color = saturate(transmission_color);

  node_light_accumulation_impl(int(light_index),
                               diffuse_light.rgb,
                               diffuse_color.rgb,
                               glossy_light.rgb,
                               glossy_color.rgb,
                               transmission_light.rgb,
                               transmission_color.rgb,
                               weight,
                               result);
}
