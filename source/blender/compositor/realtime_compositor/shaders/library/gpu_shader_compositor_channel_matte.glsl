/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

#define CMP_NODE_CHANNEL_MATTE_CS_RGB 1.0
#define CMP_NODE_CHANNEL_MATTE_CS_HSV 2.0
#define CMP_NODE_CHANNEL_MATTE_CS_YUV 3.0
#define CMP_NODE_CHANNEL_MATTE_CS_YCC 4.0

void node_composite_channel_matte(vec4 color,
                                  const float color_space,
                                  const float matte_channel,
                                  const vec2 limit_channels,
                                  float max_limit,
                                  float min_limit,
                                  out vec4 result,
                                  out float matte)
{
  vec4 channels;
  if (color_space == CMP_NODE_CHANNEL_MATTE_CS_HSV) {
    rgb_to_hsv(color, channels);
  }
  else if (color_space == CMP_NODE_CHANNEL_MATTE_CS_YUV) {
    rgba_to_yuva_itu_709(color, channels);
  }
  else if (color_space == CMP_NODE_CHANNEL_MATTE_CS_YCC) {
    rgba_to_ycca_itu_709(color, channels);
  }
  else {
    channels = color;
  }

  float matte_value = channels[int(matte_channel)];
  float limit_value = max(channels[int(limit_channels.x)], channels[int(limit_channels.y)]);

  float alpha = 1.0 - (matte_value - limit_value);
  if (alpha > max_limit) {
    alpha = color.a;
  }
  else if (alpha < min_limit) {
    alpha = 0.0;
  }
  else {
    alpha = (alpha - min_limit) / (max_limit - min_limit);
  }

  matte = min(alpha, color.a);
  result = color * matte;
}

#undef CMP_NODE_CHANNEL_MATTE_CS_RGB
#undef CMP_NODE_CHANNEL_MATTE_CS_HSV
#undef CMP_NODE_CHANNEL_MATTE_CS_YUV
#undef CMP_NODE_CHANNEL_MATTE_CS_YCC
