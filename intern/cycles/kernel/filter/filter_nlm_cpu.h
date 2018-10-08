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

#define load4_a(buf, ofs) (*((float4*) ((buf) + (ofs))))
#define load4_u(buf, ofs) load_float4((buf)+(ofs))

ccl_device_inline void kernel_filter_nlm_calc_difference(int dx, int dy,
                                                         const float *ccl_restrict weight_image,
                                                         const float *ccl_restrict variance_image,
                                                         float *difference_image,
                                                         int4 rect,
                                                         int stride,
                                                         int channel_offset,
                                                         float a,
                                                         float k_2)
{
	/* Strides need to be aligned to 16 bytes. */
	kernel_assert((stride % 4) == 0 && (channel_offset % 4) == 0);

	int aligned_lowx = rect.x & (~3);
	const int numChannels = (channel_offset > 0)? 3 : 1;
	const float4 channel_fac = make_float4(1.0f / numChannels);

	for(int y = rect.y; y < rect.w; y++) {
		int idx_p = y*stride + aligned_lowx;
		int idx_q = (y+dy)*stride + aligned_lowx + dx;
		for(int x = aligned_lowx; x < rect.z; x += 4, idx_p += 4, idx_q += 4) {
			float4 diff = make_float4(0.0f);
			for(int c = 0, chan_ofs = 0; c < numChannels; c++, chan_ofs += channel_offset) {
				/* idx_p is guaranteed to be aligned, but idx_q isn't. */
				float4 color_p = load4_a(weight_image, idx_p + chan_ofs);
				float4 color_q = load4_u(weight_image, idx_q + chan_ofs);
				float4 cdiff = color_p - color_q;
				float4 var_p = load4_a(variance_image, idx_p + chan_ofs);
				float4 var_q = load4_u(variance_image, idx_q + chan_ofs);
				diff += (cdiff*cdiff - a*(var_p + min(var_p, var_q))) / (make_float4(1e-8f) + k_2*(var_p+var_q));
			}
			load4_a(difference_image, idx_p) = diff*channel_fac;
		}
	}
}

ccl_device_inline void kernel_filter_nlm_blur(const float *ccl_restrict difference_image,
                                              float *out_image,
                                              int4 rect,
                                              int stride,
                                              int f)
{
	int aligned_lowx = round_down(rect.x, 4);
	for(int y = rect.y; y < rect.w; y++) {
		const int low = max(rect.y, y-f);
		const int high = min(rect.w, y+f+1);
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			load4_a(out_image, y*stride + x) = make_float4(0.0f);
		}
		for(int y1 = low; y1 < high; y1++) {
			for(int x = aligned_lowx; x < rect.z; x += 4) {
				load4_a(out_image, y*stride + x) += load4_a(difference_image, y1*stride + x);
			}
		}
		float fac = 1.0f/(high - low);
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			load4_a(out_image, y*stride + x) *= fac;
		}
	}
}

ccl_device_inline void nlm_blur_horizontal(const float *ccl_restrict difference_image,
                                           float *out_image,
                                           int4 rect,
                                           int stride,
                                           int f)
{
	int aligned_lowx = round_down(rect.x, 4);
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			load4_a(out_image, y*stride + x) = make_float4(0.0f);
		}
	}

	for(int dx = -f; dx <= f; dx++) {
		aligned_lowx = round_down(rect.x - min(0, dx), 4);
		int highx = rect.z - max(0, dx);
		int4 lowx4 = make_int4(rect.x - min(0, dx));
		int4 highx4 = make_int4(rect.z - max(0, dx));
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = aligned_lowx; x < highx; x += 4) {
				int4 x4 = make_int4(x) + make_int4(0, 1, 2, 3);
				int4 active = (x4 >= lowx4) & (x4 < highx4);

				float4 diff = load4_u(difference_image, y*stride + x + dx);
				load4_a(out_image, y*stride + x) += mask(active, diff);
			}
		}
	}

	aligned_lowx = round_down(rect.x, 4);
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			float4 x4 = make_float4(x) + make_float4(0.0f, 1.0f, 2.0f, 3.0f);
			float4 low = max(make_float4(rect.x), x4 - make_float4(f));
			float4 high = min(make_float4(rect.z), x4 + make_float4(f+1));
			load4_a(out_image, y*stride + x) *= rcp(high - low);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_calc_weight(const float *ccl_restrict difference_image,
                                                     float *out_image,
                                                     int4 rect,
                                                     int stride,
                                                     int f)
{
	nlm_blur_horizontal(difference_image, out_image, rect, stride, f);

	int aligned_lowx = round_down(rect.x, 4);
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			load4_a(out_image, y*stride + x) = fast_expf4(-max(load4_a(out_image, y*stride + x), make_float4(0.0f)));
		}
	}
}

ccl_device_inline void kernel_filter_nlm_update_output(int dx, int dy,
                                                       const float *ccl_restrict difference_image,
                                                       const float *ccl_restrict image,
                                                       float *temp_image,
                                                       float *out_image,
                                                       float *accum_image,
                                                       int4 rect,
                                                       int stride,
                                                       int f)
{
	nlm_blur_horizontal(difference_image, temp_image, rect, stride, f);

	int aligned_lowx = round_down(rect.x, 4);
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = aligned_lowx; x < rect.z; x += 4) {
			int4 x4 = make_int4(x) + make_int4(0, 1, 2, 3);
			int4 active = (x4 >= make_int4(rect.x)) & (x4 < make_int4(rect.z));

			int idx_p = y*stride + x, idx_q = (y+dy)*stride + (x+dx);

			float4 weight = load4_a(temp_image, idx_p);
			load4_a(accum_image, idx_p) += mask(active, weight);

			float4 val = load4_u(image, idx_q);

			load4_a(out_image, idx_p) += mask(active, weight*val);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_construct_gramian(int dx, int dy,
                                                           const float *ccl_restrict difference_image,
                                                           const float *ccl_restrict buffer,
                                                           float *transform,
                                                           int *rank,
                                                           float *XtWX,
                                                           float3 *XtWY,
                                                           int4 rect,
                                                           int4 filter_window,
                                                           int stride, int f,
                                                           int pass_stride)
{
	int4 clip_area = rect_clip(rect, filter_window);
	/* fy and fy are in filter-window-relative coordinates, while x and y are in feature-window-relative coordinates. */
	for(int y = clip_area.y; y < clip_area.w; y++) {
		for(int x = clip_area.x; x < clip_area.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += difference_image[y*stride + x1];
			}
			float weight = sum * (1.0f/(high - low));

			int storage_ofs = coord_to_local_index(filter_window, x, y);
			float  *l_transform = transform + storage_ofs*TRANSFORM_SIZE;
			float  *l_XtWX = XtWX + storage_ofs*XTWX_SIZE;
			float3 *l_XtWY = XtWY + storage_ofs*XTWY_SIZE;
			int    *l_rank = rank + storage_ofs;

			kernel_filter_construct_gramian(x, y, 1,
			                                dx, dy,
			                                stride,
			                                pass_stride,
			                                buffer,
			                                l_transform, l_rank,
			                                weight, l_XtWX, l_XtWY, 0);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_normalize(float *out_image,
                                                   const float *ccl_restrict accum_image,
                                                   int4 rect,
                                                   int w)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*w+x] /= accum_image[y*w+x];
		}
	}
}

#undef load4_a
#undef load4_u

CCL_NAMESPACE_END
