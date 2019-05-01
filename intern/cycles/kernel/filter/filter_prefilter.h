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

/**
 * First step of the shadow prefiltering, performs the shadow division and stores all data
 * in a nice and easy rectangular array that can be passed to the NLM filter.
 *
 * Calculates:
 * \param unfiltered: Contains the two half images of the shadow feature pass
 * \param sampleVariance: The sample-based variance calculated in the kernel.
 * Note: This calculation is biased in general,
 * and especially here since the variance of the ratio can only be approximated.
 * \param sampleVarianceV: Variance of the sample variance estimation, quite noisy
 * (since it's essentially the buffer variance of the two variance halves)
 * \param bufferVariance: The buffer-based variance of the shadow feature.
 * Unbiased, but quite noisy.
 */
ccl_device void kernel_filter_divide_shadow(int sample,
                                            CCL_FILTER_TILE_INFO,
                                            int x,
                                            int y,
                                            ccl_global float *unfilteredA,
                                            ccl_global float *unfilteredB,
                                            ccl_global float *sampleVariance,
                                            ccl_global float *sampleVarianceV,
                                            ccl_global float *bufferVariance,
                                            int4 rect,
                                            int buffer_pass_stride,
                                            int buffer_denoising_offset)
{
  int xtile = (x < tile_info->x[1]) ? 0 : ((x < tile_info->x[2]) ? 1 : 2);
  int ytile = (y < tile_info->y[1]) ? 0 : ((y < tile_info->y[2]) ? 1 : 2);
  int tile = ytile * 3 + xtile;

  int offset = tile_info->offsets[tile];
  int stride = tile_info->strides[tile];
  const ccl_global float *ccl_restrict center_buffer = (ccl_global float *)ccl_get_tile_buffer(
      tile);
  center_buffer += (y * stride + x + offset) * buffer_pass_stride;
  center_buffer += buffer_denoising_offset + 14;

  int buffer_w = align_up(rect.z - rect.x, 4);
  int idx = (y - rect.y) * buffer_w + (x - rect.x);
  unfilteredA[idx] = center_buffer[1] / max(center_buffer[0], 1e-7f);
  unfilteredB[idx] = center_buffer[4] / max(center_buffer[3], 1e-7f);

  float varA = center_buffer[2];
  float varB = center_buffer[5];
  int odd_sample = (sample + 1) / 2;
  int even_sample = sample / 2;

  /* Approximate variance as E[x^2] - 1/N * (E[x])^2, since online variance
   * update does not work efficiently with atomics in the kernel. */
  varA = max(0.0f, varA - unfilteredA[idx] * unfilteredA[idx] * odd_sample);
  varB = max(0.0f, varB - unfilteredB[idx] * unfilteredB[idx] * even_sample);

  varA /= max(odd_sample - 1, 1);
  varB /= max(even_sample - 1, 1);

  sampleVariance[idx] = 0.5f * (varA + varB) / sample;
  sampleVarianceV[idx] = 0.5f * (varA - varB) * (varA - varB) / (sample * sample);
  bufferVariance[idx] = 0.5f * (unfilteredA[idx] - unfilteredB[idx]) *
                        (unfilteredA[idx] - unfilteredB[idx]);
}

/* Load a regular feature from the render buffers into the denoise buffer.
 * Parameters:
 * - sample: The sample amount in the buffer, used to normalize the buffer.
 * - m_offset, v_offset: Render Buffer Pass offsets of mean and variance of the feature.
 * - x, y: Current pixel
 * - mean, variance: Target denoise buffers.
 * - rect: The prefilter area (lower pixels inclusive, upper pixels exclusive).
 */
ccl_device void kernel_filter_get_feature(int sample,
                                          CCL_FILTER_TILE_INFO,
                                          int m_offset,
                                          int v_offset,
                                          int x,
                                          int y,
                                          ccl_global float *mean,
                                          ccl_global float *variance,
                                          float scale,
                                          int4 rect,
                                          int buffer_pass_stride,
                                          int buffer_denoising_offset)
{
  int xtile = (x < tile_info->x[1]) ? 0 : ((x < tile_info->x[2]) ? 1 : 2);
  int ytile = (y < tile_info->y[1]) ? 0 : ((y < tile_info->y[2]) ? 1 : 2);
  int tile = ytile * 3 + xtile;
  ccl_global float *center_buffer = ((ccl_global float *)ccl_get_tile_buffer(tile)) +
                                    (tile_info->offsets[tile] + y * tile_info->strides[tile] + x) *
                                        buffer_pass_stride +
                                    buffer_denoising_offset;

  int buffer_w = align_up(rect.z - rect.x, 4);
  int idx = (y - rect.y) * buffer_w + (x - rect.x);

  float val = scale * center_buffer[m_offset];
  mean[idx] = val;

  if (v_offset >= 0) {
    if (sample > 1) {
      /* Approximate variance as E[x^2] - 1/N * (E[x])^2, since online variance
       * update does not work efficiently with atomics in the kernel. */
      variance[idx] = max(
          0.0f, (center_buffer[v_offset] - val * val * sample) / (sample * (sample - 1)));
    }
    else {
      /* Can't compute variance with single sample, just set it very high. */
      variance[idx] = 1e10f;
    }
  }
}

ccl_device void kernel_filter_write_feature(int sample,
                                            int x,
                                            int y,
                                            int4 buffer_params,
                                            ccl_global float *from,
                                            ccl_global float *buffer,
                                            int out_offset,
                                            int4 rect)
{
  ccl_global float *combined_buffer = buffer + (y * buffer_params.y + x + buffer_params.x) *
                                                   buffer_params.z;

  int buffer_w = align_up(rect.z - rect.x, 4);
  int idx = (y - rect.y) * buffer_w + (x - rect.x);

  combined_buffer[out_offset] = from[idx];
}

ccl_device void kernel_filter_detect_outliers(int x,
                                              int y,
                                              ccl_global float *image,
                                              ccl_global float *variance,
                                              ccl_global float *depth,
                                              ccl_global float *out,
                                              int4 rect,
                                              int pass_stride)
{
  int buffer_w = align_up(rect.z - rect.x, 4);

  int n = 0;
  float values[25];
  float pixel_variance, max_variance = 0.0f;
  for (int y1 = max(y - 2, rect.y); y1 < min(y + 3, rect.w); y1++) {
    for (int x1 = max(x - 2, rect.x); x1 < min(x + 3, rect.z); x1++) {
      int idx = (y1 - rect.y) * buffer_w + (x1 - rect.x);
      float3 color = make_float3(
          image[idx], image[idx + pass_stride], image[idx + 2 * pass_stride]);
      color = max(color, make_float3(0.0f, 0.0f, 0.0f));
      float L = average(color);

      /* Find the position of L. */
      int i;
      for (i = 0; i < n; i++) {
        if (values[i] > L)
          break;
      }
      /* Make space for L by shifting all following values to the right. */
      for (int j = n; j > i; j--) {
        values[j] = values[j - 1];
      }
      /* Insert L. */
      values[i] = L;
      n++;

      float3 pixel_var = make_float3(
          variance[idx], variance[idx + pass_stride], variance[idx + 2 * pass_stride]);
      float var = average(pixel_var);
      if ((x1 == x) && (y1 == y)) {
        pixel_variance = (pixel_var.x < 0.0f || pixel_var.y < 0.0f || pixel_var.z < 0.0f) ? -1.0f :
                                                                                            var;
      }
      else {
        max_variance = max(max_variance, var);
      }
    }
  }

  max_variance += 1e-4f;

  int idx = (y - rect.y) * buffer_w + (x - rect.x);
  float3 color = make_float3(image[idx], image[idx + pass_stride], image[idx + 2 * pass_stride]);
  color = max(color, make_float3(0.0f, 0.0f, 0.0f));
  float L = average(color);

  float ref = 2.0f * values[(int)(n * 0.75f)];

  /* Slightly offset values to avoid false positives in (almost) black areas. */
  max_variance += 1e-5f;
  ref -= 1e-5f;

  if (L > ref) {
    /* The pixel appears to be an outlier.
     * However, it may just be a legitimate highlight. Therefore, it is checked how likely it is
     * that the pixel should actually be at the reference value: If the reference is within the
     * 3-sigma interval, the pixel is assumed to be a statistical outlier. Otherwise, it is very
     * unlikely that the pixel should be darker, which indicates a legitimate highlight.
     */

    if (pixel_variance < 0.0f || pixel_variance > 9.0f * max_variance) {
      depth[idx] = -depth[idx];
      color *= ref / L;
      variance[idx] = variance[idx + pass_stride] = variance[idx + 2 * pass_stride] = max_variance;
    }
    else {
      float stddev = sqrtf(pixel_variance);
      if (L - 3 * stddev < ref) {
        /* The pixel is an outlier, so negate the depth value to mark it as one.
         * Also, scale its brightness down to the outlier threshold to avoid trouble with the NLM
         * weights. */
        depth[idx] = -depth[idx];
        float fac = ref / L;
        color *= fac;
        variance[idx] *= fac * fac;
        variance[idx + pass_stride] *= fac * fac;
        variance[idx + 2 * pass_stride] *= fac * fac;
      }
    }
  }
  out[idx] = color.x;
  out[idx + pass_stride] = color.y;
  out[idx + 2 * pass_stride] = color.z;
}

/* Combine A/B buffers.
 * Calculates the combined mean and the buffer variance. */
ccl_device void kernel_filter_combine_halves(int x,
                                             int y,
                                             ccl_global float *mean,
                                             ccl_global float *variance,
                                             ccl_global float *a,
                                             ccl_global float *b,
                                             int4 rect,
                                             int r)
{
  int buffer_w = align_up(rect.z - rect.x, 4);
  int idx = (y - rect.y) * buffer_w + (x - rect.x);

  if (mean)
    mean[idx] = 0.5f * (a[idx] + b[idx]);
  if (variance) {
    if (r == 0)
      variance[idx] = 0.25f * (a[idx] - b[idx]) * (a[idx] - b[idx]);
    else {
      variance[idx] = 0.0f;
      float values[25];
      int numValues = 0;
      for (int py = max(y - r, rect.y); py < min(y + r + 1, rect.w); py++) {
        for (int px = max(x - r, rect.x); px < min(x + r + 1, rect.z); px++) {
          int pidx = (py - rect.y) * buffer_w + (px - rect.x);
          values[numValues++] = 0.25f * (a[pidx] - b[pidx]) * (a[pidx] - b[pidx]);
        }
      }
      /* Insertion-sort the variances (fast enough for 25 elements). */
      for (int i = 1; i < numValues; i++) {
        float v = values[i];
        int j;
        for (j = i - 1; j >= 0 && values[j] > v; j--)
          values[j + 1] = values[j];
        values[j + 1] = v;
      }
      variance[idx] = values[(7 * numValues) / 8];
    }
  }
}

CCL_NAMESPACE_END
