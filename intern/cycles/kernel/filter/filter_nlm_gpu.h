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

/* Determines pixel coordinates and offset for the current thread.
 * Returns whether the thread should do any work.
 *
 * All coordinates are relative to the denoising buffer!
 *
 * Window is the rect that should be processed.
 * co is filled with (x, y, dx, dy).
 */
ccl_device_inline bool get_nlm_coords_window(
    int w, int h, int r, int stride, int4 *rect, int4 *co, int *ofs, int4 window)
{
  /* Determine the pixel offset that this thread should apply. */
  int s = 2 * r + 1;
  int si = ccl_global_id(1);
  int sx = si % s;
  int sy = si / s;
  if (sy >= s) {
    return false;
  }

  /* Pixels still need to lie inside the denoising buffer after applying the offset,
   * so determine the area for which this is the case. */
  int dx = sx - r;
  int dy = sy - r;

  *rect = make_int4(max(0, -dx), max(0, -dy), w - max(0, dx), h - max(0, dy));

  /* Find the intersection of the area that we want to process (window) and the area
   * that can be processed (rect) to get the final area for this offset. */
  int4 clip_area = rect_clip(window, *rect);

  /* If the radius is larger than one of the sides of the window,
   * there will be shifts for which there is no usable pixel at all. */
  if (!rect_is_valid(clip_area)) {
    return false;
  }

  /* Map the linear thread index to pixels inside the clip area. */
  int x, y;
  if (!local_index_to_coord(clip_area, ccl_global_id(0), &x, &y)) {
    return false;
  }

  *co = make_int4(x, y, dx, dy);

  *ofs = (sy * s + sx) * stride;

  return true;
}

ccl_device_inline bool get_nlm_coords(
    int w, int h, int r, int stride, int4 *rect, int4 *co, int *ofs)
{
  return get_nlm_coords_window(w, h, r, stride, rect, co, ofs, make_int4(0, 0, w, h));
}

ccl_device_inline void kernel_filter_nlm_calc_difference(
    int x,
    int y,
    int dx,
    int dy,
    const ccl_global float *ccl_restrict weight_image,
    const ccl_global float *ccl_restrict variance_image,
    const ccl_global float *ccl_restrict scale_image,
    ccl_global float *difference_image,
    int4 rect,
    int stride,
    int channel_offset,
    int frame_offset,
    float a,
    float k_2)
{
  int idx_p = y * stride + x, idx_q = (y + dy) * stride + (x + dx) + frame_offset;
  int numChannels = channel_offset ? 3 : 1;

  float diff = 0.0f;
  float scale_fac = 1.0f;
  if (scale_image) {
    scale_fac = clamp(scale_image[idx_p] / scale_image[idx_q], 0.25f, 4.0f);
  }

  for (int c = 0; c < numChannels; c++, idx_p += channel_offset, idx_q += channel_offset) {
    float cdiff = weight_image[idx_p] - scale_fac * weight_image[idx_q];
    float pvar = variance_image[idx_p];
    float qvar = sqr(scale_fac) * variance_image[idx_q];
    diff += (cdiff * cdiff - a * (pvar + min(pvar, qvar))) / (1e-8f + k_2 * (pvar + qvar));
  }
  if (numChannels > 1) {
    diff *= 1.0f / numChannels;
  }
  difference_image[y * stride + x] = diff;
}

ccl_device_inline void kernel_filter_nlm_blur(int x,
                                              int y,
                                              const ccl_global float *ccl_restrict
                                                  difference_image,
                                              ccl_global float *out_image,
                                              int4 rect,
                                              int stride,
                                              int f)
{
  float sum = 0.0f;
  const int low = max(rect.y, y - f);
  const int high = min(rect.w, y + f + 1);
  for (int y1 = low; y1 < high; y1++) {
    sum += difference_image[y1 * stride + x];
  }
  sum *= 1.0f / (high - low);
  out_image[y * stride + x] = sum;
}

ccl_device_inline void kernel_filter_nlm_calc_weight(int x,
                                                     int y,
                                                     const ccl_global float *ccl_restrict
                                                         difference_image,
                                                     ccl_global float *out_image,
                                                     int4 rect,
                                                     int stride,
                                                     int f)
{
  float sum = 0.0f;
  const int low = max(rect.x, x - f);
  const int high = min(rect.z, x + f + 1);
  for (int x1 = low; x1 < high; x1++) {
    sum += difference_image[y * stride + x1];
  }
  sum *= 1.0f / (high - low);
  out_image[y * stride + x] = fast_expf(-max(sum, 0.0f));
}

ccl_device_inline void kernel_filter_nlm_update_output(int x,
                                                       int y,
                                                       int dx,
                                                       int dy,
                                                       const ccl_global float *ccl_restrict
                                                           difference_image,
                                                       const ccl_global float *ccl_restrict image,
                                                       ccl_global float *out_image,
                                                       ccl_global float *accum_image,
                                                       int4 rect,
                                                       int channel_offset,
                                                       int stride,
                                                       int f)
{
  float sum = 0.0f;
  const int low = max(rect.x, x - f);
  const int high = min(rect.z, x + f + 1);
  for (int x1 = low; x1 < high; x1++) {
    sum += difference_image[y * stride + x1];
  }
  sum *= 1.0f / (high - low);

  int idx_p = y * stride + x, idx_q = (y + dy) * stride + (x + dx);
  if (out_image) {
    atomic_add_and_fetch_float(accum_image + idx_p, sum);

    float val = image[idx_q];
    if (channel_offset) {
      val += image[idx_q + channel_offset];
      val += image[idx_q + 2 * channel_offset];
      val *= 1.0f / 3.0f;
    }
    atomic_add_and_fetch_float(out_image + idx_p, sum * val);
  }
  else {
    accum_image[idx_p] = sum;
  }
}

ccl_device_inline void kernel_filter_nlm_construct_gramian(
    int x,
    int y,
    int dx,
    int dy,
    int t,
    const ccl_global float *ccl_restrict difference_image,
    const ccl_global float *ccl_restrict buffer,
    const ccl_global float *ccl_restrict transform,
    ccl_global int *rank,
    ccl_global float *XtWX,
    ccl_global float3 *XtWY,
    int4 rect,
    int4 filter_window,
    int stride,
    int f,
    int pass_stride,
    int frame_offset,
    bool use_time,
    int localIdx)
{
  const int low = max(rect.x, x - f);
  const int high = min(rect.z, x + f + 1);
  float sum = 0.0f;
  for (int x1 = low; x1 < high; x1++) {
    sum += difference_image[y * stride + x1];
  }
  float weight = sum * (1.0f / (high - low));

  /* Reconstruction data is only stored for pixels inside the filter window,
   * so compute the pixels's index in there. */
  int storage_ofs = coord_to_local_index(filter_window, x, y);
  transform += storage_ofs;
  rank += storage_ofs;
  XtWX += storage_ofs;
  XtWY += storage_ofs;

  kernel_filter_construct_gramian(x,
                                  y,
                                  rect_size(filter_window),
                                  dx,
                                  dy,
                                  t,
                                  stride,
                                  pass_stride,
                                  frame_offset,
                                  use_time,
                                  buffer,
                                  transform,
                                  rank,
                                  weight,
                                  XtWX,
                                  XtWY,
                                  localIdx);
}

ccl_device_inline void kernel_filter_nlm_normalize(int x,
                                                   int y,
                                                   ccl_global float *out_image,
                                                   const ccl_global float *ccl_restrict
                                                       accum_image,
                                                   int stride)
{
  out_image[y * stride + x] /= accum_image[y * stride + x];
}

CCL_NAMESPACE_END
