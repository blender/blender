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

ccl_device_inline void kernel_filter_construct_gramian(int x,
                                                       int y,
                                                       int storage_stride,
                                                       int dx,
                                                       int dy,
                                                       int t,
                                                       int buffer_stride,
                                                       int pass_stride,
                                                       int frame_offset,
                                                       bool use_time,
                                                       const ccl_global float *ccl_restrict buffer,
                                                       const ccl_global float *ccl_restrict
                                                           transform,
                                                       ccl_global int *rank,
                                                       float weight,
                                                       ccl_global float *XtWX,
                                                       ccl_global float3 *XtWY,
                                                       int localIdx)
{
  if (weight < 1e-3f) {
    return;
  }

  int p_offset = y * buffer_stride + x;
  int q_offset = (y + dy) * buffer_stride + (x + dx) + frame_offset;

#ifdef __KERNEL_GPU__
  const int stride = storage_stride;
#else
  const int stride = 1;
  (void)storage_stride;
#endif

#ifdef __KERNEL_CUDA__
  ccl_local float shared_design_row[(DENOISE_FEATURES + 1) * CCL_MAX_LOCAL_SIZE];
  ccl_local_param float *design_row = shared_design_row + localIdx * (DENOISE_FEATURES + 1);
#else
  float design_row[DENOISE_FEATURES + 1];
#endif

  float3 q_color = filter_get_color(buffer + q_offset, pass_stride);

  /* If the pixel was flagged as an outlier during prefiltering, skip it. */
  if (ccl_get_feature(buffer + q_offset, 0) < 0.0f) {
    return;
  }

  filter_get_design_row_transform(make_int3(x, y, t),
                                  buffer + p_offset,
                                  make_int3(x + dx, y + dy, t),
                                  buffer + q_offset,
                                  pass_stride,
                                  *rank,
                                  design_row,
                                  transform,
                                  stride,
                                  use_time);

#ifdef __KERNEL_GPU__
  math_trimatrix_add_gramian_strided(XtWX, (*rank) + 1, design_row, weight, stride);
  math_vec3_add_strided(XtWY, (*rank) + 1, design_row, weight * q_color, stride);
#else
  math_trimatrix_add_gramian(XtWX, (*rank) + 1, design_row, weight);
  math_vec3_add(XtWY, (*rank) + 1, design_row, weight * q_color);
#endif
}

ccl_device_inline void kernel_filter_finalize(int x,
                                              int y,
                                              ccl_global float *buffer,
                                              ccl_global int *rank,
                                              int storage_stride,
                                              ccl_global float *XtWX,
                                              ccl_global float3 *XtWY,
                                              int4 buffer_params,
                                              int sample)
{
#ifdef __KERNEL_GPU__
  const int stride = storage_stride;
#else
  const int stride = 1;
  (void)storage_stride;
#endif

  if (XtWX[0] < 1e-3f) {
    /* There is not enough information to determine a denoised result.
     * As a fallback, keep the original value of the pixel. */
    return;
  }

  /* The weighted average of pixel colors (essentially, the NLM-filtered image).
   * In case the solution of the linear model fails due to numerical issues or
   * returns non-sensical negative values, fall back to this value. */
  float3 mean_color = XtWY[0] / XtWX[0];

  math_trimatrix_vec3_solve(XtWX, XtWY, (*rank) + 1, stride);

  float3 final_color = XtWY[0];
  if (!isfinite3_safe(final_color) ||
      (final_color.x < -0.01f || final_color.y < -0.01f || final_color.z < -0.01f)) {
    final_color = mean_color;
  }

  /* Clamp pixel value to positive values and reverse the highlight compression transform. */
  final_color = color_highlight_uncompress(max(final_color, make_float3(0.0f, 0.0f, 0.0f)));

  ccl_global float *combined_buffer = buffer + (y * buffer_params.y + x + buffer_params.x) *
                                                   buffer_params.z;
  if (buffer_params.w >= 0) {
    final_color *= sample;
    if (buffer_params.w > 0) {
      final_color.x += combined_buffer[buffer_params.w + 0];
      final_color.y += combined_buffer[buffer_params.w + 1];
      final_color.z += combined_buffer[buffer_params.w + 2];
    }
  }
  combined_buffer[0] = final_color.x;
  combined_buffer[1] = final_color.y;
  combined_buffer[2] = final_color.z;
}

CCL_NAMESPACE_END
