/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

#define CMP_NODE_CHANNEL_MATTE_CS_RGB 0
#define CMP_NODE_CHANNEL_MATTE_CS_HSV 1
#define CMP_NODE_CHANNEL_MATTE_CS_YUV 2
#define CMP_NODE_CHANNEL_MATTE_CS_YCC 3

#define CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE 0
#define CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX 1

float3 compute_channels(const float4 color, const int color_space)
{
  switch (color_space) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB: {
      return color.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_HSV: {
      float4 hsv;
      rgb_to_hsv(color, hsv);
      return hsv.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_YUV: {
      float4 yuv;
      rgba_to_yuva_itu_709(color, yuv);
      return yuv.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_YCC: {
      float4 ycc;
      rgba_to_ycca_itu_709(color, ycc);
      return ycc.xyz();
    }
  }

  return color.xyz();
}

int get_channel_index(const int color_space,
                      const int rgb_channel,
                      const int hsv_channel,
                      const int yuv_channel,
                      const int ycc_channel)
{
  switch (color_space) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      return rgb_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV:
      return hsv_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV:
      return yuv_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC:
      return ycc_channel;
  }

  return 0;
}

/* Compute the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Max, if it is a single limit channel, store it in both limit channels, because
 * the maximum of two identical values is the same value. */
int2 compute_limit_channels(const int limit_method,
                            const int matte_channel,
                            const int limit_channel)
{
  /* If the algorithm is Max, store the indices of the other two channels other than the matte
   * channel. */
  if (limit_method == CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX) {
    return int2((matte_channel + 1) % 3, (matte_channel + 2) % 3);
  }

  /* If the algorithm is Single, store the index of the limit channel in both channels. */
  return int2(limit_channel);
}

void node_composite_channel_matte(const float4 color,
                                  const float minimum,
                                  const float maximum,
                                  const float color_space,
                                  const float rgb_key_channel,
                                  const float hsv_key_channel,
                                  const float yuv_key_channel,
                                  const float ycc_key_channel,
                                  const float limit_method,
                                  const float rgb_limit_channel,
                                  const float hsv_limit_channel,
                                  const float yuv_limit_channel,
                                  const float ycc_limit_channel,
                                  out float4 output_color,
                                  out float matte)
{
  const float3 channels = compute_channels(color, int(color_space));
  const int matte_channel = get_channel_index(int(color_space),
                                              int(rgb_key_channel),
                                              int(hsv_key_channel),
                                              int(yuv_key_channel),
                                              int(ycc_key_channel));
  const int limit_channel = get_channel_index(int(color_space),
                                              int(rgb_limit_channel),
                                              int(hsv_limit_channel),
                                              int(yuv_limit_channel),
                                              int(ycc_limit_channel));
  const int2 limit_channels = compute_limit_channels(
      int(limit_method), int(matte_channel), int(limit_channel));

  float matte_value = channels[int(matte_channel)];
  float limit_value = max(channels[int(limit_channels.x)], channels[int(limit_channels.y)]);

  float alpha = 1.0f - (matte_value - limit_value);
  if (alpha > maximum) {
    alpha = color.a;
  }
  else if (alpha < minimum) {
    alpha = 0.0f;
  }
  else {
    alpha = (alpha - minimum) / (maximum - minimum);
  }

  matte = min(alpha, color.a);
  output_color = color * matte;
}

#undef CMP_NODE_CHANNEL_MATTE_CS_RGB
#undef CMP_NODE_CHANNEL_MATTE_CS_HSV
#undef CMP_NODE_CHANNEL_MATTE_CS_YUV
#undef CMP_NODE_CHANNEL_MATTE_CS_YCC

#undef CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGL
#undef CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX
