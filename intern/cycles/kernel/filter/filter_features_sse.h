/*
 * Copyright 2011-2017 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

#define ccl_get_feature_sse(pass) load_float4(buffer + (pass)*pass_stride)

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y), 4 at a time.
 * pixel_buffer always points to the first of the 4 current pixel in the first pass.
 * x4 and y4 contain the coordinates of the four pixels, active_pixels contains a mask that's set
 * for all pixels within the window. Repeat the loop for every secondary frame if there are any. */
#define FOR_PIXEL_WINDOW_SSE \
  for (int frame = 0; frame < tile_info->num_frames; frame++) { \
    pixel.z = tile_info->frames[frame]; \
    pixel_buffer = buffer + (low.y - rect.y) * buffer_w + (low.x - rect.x) + \
                   frame * frame_stride; \
    float4 t4 = make_float4(pixel.z); \
    for (pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
      float4 y4 = make_float4(pixel.y); \
      for (pixel.x = low.x; pixel.x < high.x; pixel.x += 4, pixel_buffer += 4) { \
        float4 x4 = make_float4(pixel.x) + make_float4(0.0f, 1.0f, 2.0f, 3.0f); \
        int4 active_pixels = x4 < make_float4(high.x);

#define END_FOR_PIXEL_WINDOW_SSE \
  } \
  pixel_buffer += buffer_w - (high.x - low.x); \
  } \
  }

ccl_device_inline void filter_get_features_sse(float4 x,
                                               float4 y,
                                               float4 t,
                                               int4 active_pixels,
                                               const float *ccl_restrict buffer,
                                               float4 *features,
                                               bool use_time,
                                               const float4 *ccl_restrict mean,
                                               int pass_stride)
{
  int num_features = use_time ? 11 : 10;

  features[0] = x;
  features[1] = y;
  features[2] = fabs(ccl_get_feature_sse(0));
  features[3] = ccl_get_feature_sse(1);
  features[4] = ccl_get_feature_sse(2);
  features[5] = ccl_get_feature_sse(3);
  features[6] = ccl_get_feature_sse(4);
  features[7] = ccl_get_feature_sse(5);
  features[8] = ccl_get_feature_sse(6);
  features[9] = ccl_get_feature_sse(7);
  if (use_time) {
    features[10] = t;
  }

  if (mean) {
    for (int i = 0; i < num_features; i++) {
      features[i] = features[i] - mean[i];
    }
  }
  for (int i = 0; i < num_features; i++) {
    features[i] = mask(active_pixels, features[i]);
  }
}

ccl_device_inline void filter_get_feature_scales_sse(float4 x,
                                                     float4 y,
                                                     float4 t,
                                                     int4 active_pixels,
                                                     const float *ccl_restrict buffer,
                                                     float4 *scales,
                                                     bool use_time,
                                                     const float4 *ccl_restrict mean,
                                                     int pass_stride)
{
  scales[0] = fabs(x - mean[0]);
  scales[1] = fabs(y - mean[1]);
  scales[2] = fabs(fabs(ccl_get_feature_sse(0)) - mean[2]);
  scales[3] = sqr(ccl_get_feature_sse(1) - mean[3]) + sqr(ccl_get_feature_sse(2) - mean[4]) +
              sqr(ccl_get_feature_sse(3) - mean[5]);
  scales[4] = fabs(ccl_get_feature_sse(4) - mean[6]);
  scales[5] = sqr(ccl_get_feature_sse(5) - mean[7]) + sqr(ccl_get_feature_sse(6) - mean[8]) +
              sqr(ccl_get_feature_sse(7) - mean[9]);
  if (use_time) {
    scales[6] = fabs(t - mean[10]);
  }

  for (int i = 0; i < (use_time ? 7 : 6); i++)
    scales[i] = mask(active_pixels, scales[i]);
}

ccl_device_inline void filter_calculate_scale_sse(float4 *scale, bool use_time)
{
  scale[0] = rcp(max(reduce_max(scale[0]), make_float4(0.01f)));
  scale[1] = rcp(max(reduce_max(scale[1]), make_float4(0.01f)));
  scale[2] = rcp(max(reduce_max(scale[2]), make_float4(0.01f)));
  if (use_time) {
    scale[10] = rcp(max(reduce_max(scale[6]), make_float4(0.01f)));
    ;
  }
  scale[6] = rcp(max(reduce_max(scale[4]), make_float4(0.01f)));
  scale[7] = scale[8] = scale[9] = rcp(max(reduce_max(sqrt(scale[5])), make_float4(0.01f)));
  scale[3] = scale[4] = scale[5] = rcp(max(reduce_max(sqrt(scale[3])), make_float4(0.01f)));
}

CCL_NAMESPACE_END
