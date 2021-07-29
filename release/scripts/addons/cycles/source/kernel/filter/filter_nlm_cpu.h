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
                                                         int w,
                                                         int channel_offset,
                                                         float a,
                                                         float k_2)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			float diff = 0.0f;
			int numChannels = channel_offset? 3 : 1;
			for(int c = 0; c < numChannels; c++) {
				float cdiff = weight_image[c*channel_offset + y*w+x] - weight_image[c*channel_offset + (y+dy)*w+(x+dx)];
				float pvar = variance_image[c*channel_offset + y*w+x];
				float qvar = variance_image[c*channel_offset + (y+dy)*w+(x+dx)];
				diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
			}
			if(numChannels > 1) {
				diff *= 1.0f/numChannels;
			}
			difference_image[y*w+x] = diff;
		}
	}
}

ccl_device_inline void kernel_filter_nlm_blur(const float *ccl_restrict difference_image,
                                              float *out_image,
                                              int4 rect,
                                              int w,
                                              int f)
{
#ifdef __KERNEL_SSE3__
	int aligned_lowx = (rect.x & ~(3));
	int aligned_highx = ((rect.z + 3) & ~(3));
#endif
	for(int y = rect.y; y < rect.w; y++) {
		const int low = max(rect.y, y-f);
		const int high = min(rect.w, y+f+1);
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*w+x] = 0.0f;
		}
		for(int y1 = low; y1 < high; y1++) {
#ifdef __KERNEL_SSE3__
			for(int x = aligned_lowx; x < aligned_highx; x+=4) {
				_mm_store_ps(out_image + y*w+x, _mm_add_ps(_mm_load_ps(out_image + y*w+x), _mm_load_ps(difference_image + y1*w+x)));
			}
#else
			for(int x = rect.x; x < rect.z; x++) {
				out_image[y*w+x] += difference_image[y1*w+x];
			}
#endif
		}
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*w+x] *= 1.0f/(high - low);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_calc_weight(const float *ccl_restrict difference_image,
                                                     float *out_image,
                                                     int4 rect,
                                                     int w,
                                                     int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			out_image[y*w+x] = 0.0f;
		}
	}
	for(int dx = -f; dx <= f; dx++) {
		int pos_dx = max(0, dx);
		int neg_dx = min(0, dx);
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x-neg_dx; x < rect.z-pos_dx; x++) {
				out_image[y*w+x] += difference_image[y*w+dx+x];
			}
		}
	}
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			out_image[y*w+x] = fast_expf(-max(out_image[y*w+x] * (1.0f/(high - low)), 0.0f));
		}
	}
}

ccl_device_inline void kernel_filter_nlm_update_output(int dx, int dy,
                                                       const float *ccl_restrict difference_image,
                                                       const float *ccl_restrict image,
                                                       float *out_image,
                                                       float *accum_image,
                                                       int4 rect,
                                                       int w,
                                                       int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += difference_image[y*w+x1];
			}
			float weight = sum * (1.0f/(high - low));
			accum_image[y*w+x] += weight;
			out_image[y*w+x] += weight*image[(y+dy)*w+(x+dx)];
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
                                                           int4 filter_rect,
                                                           int w, int h, int f,
                                                           int pass_stride)
{
	/* fy and fy are in filter-window-relative coordinates, while x and y are in feature-window-relative coordinates. */
	for(int fy = max(0, rect.y-filter_rect.y); fy < min(filter_rect.w, rect.w-filter_rect.y); fy++) {
		int y = fy + filter_rect.y;
		for(int fx = max(0, rect.x-filter_rect.x); fx < min(filter_rect.z, rect.z-filter_rect.x); fx++) {
			int x = fx + filter_rect.x;
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += difference_image[y*w+x1];
			}
			float weight = sum * (1.0f/(high - low));

			int storage_ofs = fy*filter_rect.z + fx;
			float  *l_transform = transform + storage_ofs*TRANSFORM_SIZE;
			float  *l_XtWX = XtWX + storage_ofs*XTWX_SIZE;
			float3 *l_XtWY = XtWY + storage_ofs*XTWY_SIZE;
			int    *l_rank = rank + storage_ofs;

			kernel_filter_construct_gramian(x, y, 1,
			                                dx, dy, w, h,
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
