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
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			float diff = 0.0f;
			int numChannels = channel_offset? 3 : 1;
			for(int c = 0; c < numChannels; c++) {
				float cdiff = weight_image[c*channel_offset + y*stride + x] - weight_image[c*channel_offset + (y+dy)*stride + (x+dx)];
				float pvar = variance_image[c*channel_offset + y*stride + x];
				float qvar = variance_image[c*channel_offset + (y+dy)*stride + (x+dx)];
				diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
			}
			if(numChannels > 1) {
				diff *= 1.0f/numChannels;
			}
			difference_image[y*stride + x] = diff;
		}
	}
}

ccl_device_inline void kernel_filter_nlm_blur(const float *ccl_restrict difference_image,
                                              float *out_image,
                                              int4 rect,
                                              int stride,
                                              int f)
{
	int aligned_lowx = rect.x / 4;
	int aligned_highx = (rect.z + 3) / 4;
	for(int y = rect.y; y < rect.w; y++) {
		const int low = max(rect.y, y-f);
		const int high = min(rect.w, y+f+1);
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*stride + x] = 0.0f;
		}
		for(int y1 = low; y1 < high; y1++) {
			float4* out_image4 = (float4*)(out_image + y*stride);
			float4* difference_image4 = (float4*)(difference_image + y1*stride);
			for(int x = aligned_lowx; x < aligned_highx; x++) {
				out_image4[x] += difference_image4[x];
			}
		}
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*stride + x] *= 1.0f/(high - low);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_calc_weight(const float *ccl_restrict difference_image,
                                                     float *out_image,
                                                     int4 rect,
                                                     int stride,
                                                     int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*stride + x] = 0.0f;
		}
	}
	for(int dx = -f; dx <= f; dx++) {
		int pos_dx = max(0, dx);
		int neg_dx = min(0, dx);
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x-neg_dx; x < rect.z-pos_dx; x++) {
				out_image[y*stride + x] += difference_image[y*stride + x+dx];
			}
		}
	}
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			out_image[y*stride + x] = fast_expf(-max(out_image[y*stride + x] * (1.0f/(high - low)), 0.0f));
		}
	}
}

ccl_device_inline void kernel_filter_nlm_update_output(int dx, int dy,
                                                       const float *ccl_restrict difference_image,
                                                       const float *ccl_restrict image,
                                                       float *out_image,
                                                       float *accum_image,
                                                       int4 rect,
                                                       int stride,
                                                       int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += difference_image[y*stride + x1];
			}
			float weight = sum * (1.0f/(high - low));
			accum_image[y*stride + x] += weight;
			out_image[y*stride + x] += weight*image[(y+dy)*stride + (x+dx)];
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

CCL_NAMESPACE_END
