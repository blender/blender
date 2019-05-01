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

ccl_device void kernel_filter_construct_transform(const float *ccl_restrict buffer,
                                                  CCL_FILTER_TILE_INFO,
                                                  int x,
                                                  int y,
                                                  int4 rect,
                                                  int pass_stride,
                                                  int frame_stride,
                                                  bool use_time,
                                                  float *transform,
                                                  int *rank,
                                                  int radius,
                                                  float pca_threshold)
{
  int buffer_w = align_up(rect.z - rect.x, 4);

  float4 features[DENOISE_FEATURES];
  const float *ccl_restrict pixel_buffer;
  int3 pixel;

  int num_features = use_time ? 11 : 10;

  /* === Calculate denoising window. === */
  int2 low = make_int2(max(rect.x, x - radius), max(rect.y, y - radius));
  int2 high = make_int2(min(rect.z, x + radius + 1), min(rect.w, y + radius + 1));
  int num_pixels = (high.y - low.y) * (high.x - low.x) * tile_info->num_frames;

  /* === Shift feature passes to have mean 0. === */
  float4 feature_means[DENOISE_FEATURES];
  math_vector_zero_sse(feature_means, num_features);
  FOR_PIXEL_WINDOW_SSE
  {
    filter_get_features_sse(
        x4, y4, t4, active_pixels, pixel_buffer, features, use_time, NULL, pass_stride);
    math_vector_add_sse(feature_means, num_features, features);
  }
  END_FOR_PIXEL_WINDOW_SSE

  float4 pixel_scale = make_float4(1.0f / num_pixels);
  for (int i = 0; i < num_features; i++) {
    feature_means[i] = reduce_add(feature_means[i]) * pixel_scale;
  }

  /* === Scale the shifted feature passes to a range of [-1; 1] ===
   * Will be baked into the transform later. */
  float4 feature_scale[DENOISE_FEATURES];
  math_vector_zero_sse(feature_scale, num_features);
  FOR_PIXEL_WINDOW_SSE
  {
    filter_get_feature_scales_sse(
        x4, y4, t4, active_pixels, pixel_buffer, features, use_time, feature_means, pass_stride);
    math_vector_max_sse(feature_scale, features, num_features);
  }
  END_FOR_PIXEL_WINDOW_SSE

  filter_calculate_scale_sse(feature_scale, use_time);

  /* === Generate the feature transformation. ===
   * This transformation maps the num_features-dimentional feature space to a reduced feature
   * (r-feature) space which generally has fewer dimensions. This mainly helps to prevent
   * overfitting. */
  float4 feature_matrix_sse[DENOISE_FEATURES * DENOISE_FEATURES];
  math_matrix_zero_sse(feature_matrix_sse, num_features);
  FOR_PIXEL_WINDOW_SSE
  {
    filter_get_features_sse(
        x4, y4, t4, active_pixels, pixel_buffer, features, use_time, feature_means, pass_stride);
    math_vector_mul_sse(features, num_features, feature_scale);
    math_matrix_add_gramian_sse(feature_matrix_sse, num_features, features, make_float4(1.0f));
  }
  END_FOR_PIXEL_WINDOW_SSE

  float feature_matrix[DENOISE_FEATURES * DENOISE_FEATURES];
  math_matrix_hsum(feature_matrix, num_features, feature_matrix_sse);

  math_matrix_jacobi_eigendecomposition(feature_matrix, transform, num_features, 1);

  *rank = 0;
  /* Prevent overfitting when a small window is used. */
  int max_rank = min(num_features, num_pixels / 3);
  if (pca_threshold < 0.0f) {
    float threshold_energy = 0.0f;
    for (int i = 0; i < num_features; i++) {
      threshold_energy += feature_matrix[i * num_features + i];
    }
    threshold_energy *= 1.0f - (-pca_threshold);

    float reduced_energy = 0.0f;
    for (int i = 0; i < max_rank; i++, (*rank)++) {
      if (i >= 2 && reduced_energy >= threshold_energy)
        break;
      float s = feature_matrix[i * num_features + i];
      reduced_energy += s;
    }
  }
  else {
    for (int i = 0; i < max_rank; i++, (*rank)++) {
      float s = feature_matrix[i * num_features + i];
      if (i >= 2 && sqrtf(s) < pca_threshold)
        break;
    }
  }

  math_matrix_transpose(transform, num_features, 1);

  /* Bake the feature scaling into the transformation matrix. */
  for (int i = 0; i < num_features; i++) {
    math_vector_scale(transform + i * num_features, feature_scale[i][0], *rank);
  }
}

CCL_NAMESPACE_END
