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

ccl_device_inline void kernel_filter_nlm_calc_difference(int dx, int dy, float ccl_restrict_ptr weightImage, float ccl_restrict_ptr varianceImage, float *differenceImage, int4 rect, int w, int channel_offset, float a, float k_2)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			float diff = 0.0f;
			int numChannels = channel_offset? 3 : 1;
			for(int c = 0; c < numChannels; c++) {
				float cdiff = weightImage[c*channel_offset + y*w+x] - weightImage[c*channel_offset + (y+dy)*w+(x+dx)];
				float pvar = varianceImage[c*channel_offset + y*w+x];
				float qvar = varianceImage[c*channel_offset + (y+dy)*w+(x+dx)];
				diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
			}
			if(numChannels > 1) {
				diff *= 1.0f/numChannels;
			}
			differenceImage[y*w+x] = diff;
		}
	}
}

ccl_device_inline void kernel_filter_nlm_blur(float ccl_restrict_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
#ifdef __KERNEL_SSE3__
	int aligned_lowx = (rect.x & ~(3));
	int aligned_highx = ((rect.z + 3) & ~(3));
#endif
	for(int y = rect.y; y < rect.w; y++) {
		const int low = max(rect.y, y-f);
		const int high = min(rect.w, y+f+1);
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] = 0.0f;
		}
		for(int y1 = low; y1 < high; y1++) {
#ifdef __KERNEL_SSE3__
			for(int x = aligned_lowx; x < aligned_highx; x+=4) {
				_mm_store_ps(outImage + y*w+x, _mm_add_ps(_mm_load_ps(outImage + y*w+x), _mm_load_ps(differenceImage + y1*w+x)));
			}
#else
			for(int x = rect.x; x < rect.z; x++) {
				outImage[y*w+x] += differenceImage[y1*w+x];
			}
#endif
		}
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] *= 1.0f/(high - low);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_calc_weight(float ccl_restrict_ptr differenceImage, float *outImage, int4 rect, int w, int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] = 0.0f;
		}
	}
	for(int dx = -f; dx <= f; dx++) {
		int pos_dx = max(0, dx);
		int neg_dx = min(0, dx);
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x-neg_dx; x < rect.z-pos_dx; x++) {
				outImage[y*w+x] += differenceImage[y*w+dx+x];
			}
		}
	}
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			outImage[y*w+x] = expf(-max(outImage[y*w+x] * (1.0f/(high - low)), 0.0f));
		}
	}
}

ccl_device_inline void kernel_filter_nlm_update_output(int dx, int dy, float ccl_restrict_ptr differenceImage, float ccl_restrict_ptr image, float *outImage, float *accumImage, int4 rect, int w, int f)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			const int low = max(rect.x, x-f);
			const int high = min(rect.z, x+f+1);
			float sum = 0.0f;
			for(int x1 = low; x1 < high; x1++) {
				sum += differenceImage[y*w+x1];
			}
			float weight = sum * (1.0f/(high - low));
			accumImage[y*w+x] += weight;
			outImage[y*w+x] += weight*image[(y+dy)*w+(x+dx)];
		}
	}
}

ccl_device_inline void kernel_filter_nlm_construct_gramian(int dx, int dy,
                                                           float ccl_restrict_ptr differenceImage,
                                                           float ccl_restrict_ptr buffer,
                                                           float *color_pass,
                                                           float *variance_pass,
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
				sum += differenceImage[y*w+x1];
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
			                                color_pass, variance_pass,
			                                l_transform, l_rank,
			                                weight, l_XtWX, l_XtWY, 0);
		}
	}
}

ccl_device_inline void kernel_filter_nlm_normalize(float *outImage, float ccl_restrict_ptr accumImage, int4 rect, int w)
{
	for(int y = rect.y; y < rect.w; y++) {
		for(int x = rect.x; x < rect.z; x++) {
			outImage[y*w+x] /= accumImage[y*w+x];
		}
	}
}

CCL_NAMESPACE_END
