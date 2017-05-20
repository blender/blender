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

#define ccl_get_feature_sse(pass) _mm_loadu_ps(buffer + (pass)*pass_stride)

/* Loop over the pixels in the range [low.x, high.x) x [low.y, high.y), 4 at a time.
 * pixel_buffer always points to the first of the 4 current pixel in the first pass.
 * x4 and y4 contain the coordinates of the four pixels, active_pixels contains a mask that's set for all pixels within the window. */

#define FOR_PIXEL_WINDOW_SSE     pixel_buffer = buffer + (low.y - rect.y)*buffer_w + (low.x - rect.x); \
                                 for(pixel.y = low.y; pixel.y < high.y; pixel.y++) { \
                                     __m128 y4 = _mm_set1_ps(pixel.y); \
                                     for(pixel.x = low.x; pixel.x < high.x; pixel.x += 4, pixel_buffer += 4) { \
                                         __m128 x4 = _mm_add_ps(_mm_set1_ps(pixel.x), _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)); \
                                         __m128 active_pixels = _mm_cmplt_ps(x4, _mm_set1_ps(high.x));

#define END_FOR_PIXEL_WINDOW_SSE     } \
                                     pixel_buffer += buffer_w - (pixel.x - low.x); \
                                 }

ccl_device_inline void filter_get_features_sse(__m128 x, __m128 y,
                                               __m128 active_pixels,
                                               const float *ccl_restrict buffer,
                                               __m128 *features,
                                               const __m128 *ccl_restrict mean,
                                               int pass_stride)
{
	features[0] = x;
	features[1] = y;
	features[2] = _mm_fabs_ps(ccl_get_feature_sse(0));
	features[3] = ccl_get_feature_sse(1);
	features[4] = ccl_get_feature_sse(2);
	features[5] = ccl_get_feature_sse(3);
	features[6] = ccl_get_feature_sse(4);
	features[7] = ccl_get_feature_sse(5);
	features[8] = ccl_get_feature_sse(6);
	features[9] = ccl_get_feature_sse(7);
	if(mean) {
		for(int i = 0; i < DENOISE_FEATURES; i++)
			features[i] = _mm_sub_ps(features[i], mean[i]);
	}
	for(int i = 0; i < DENOISE_FEATURES; i++)
		features[i] = _mm_mask_ps(features[i], active_pixels);
}

ccl_device_inline void filter_get_feature_scales_sse(__m128 x, __m128 y,
                                                     __m128 active_pixels,
                                                     const float *ccl_restrict buffer,
                                                     __m128 *scales,
                                                     const __m128 *ccl_restrict mean,
                                                     int pass_stride)
{
	scales[0] = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(x, mean[0])), active_pixels);
	scales[1] = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(y, mean[1])), active_pixels);

	scales[2] = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(_mm_fabs_ps(ccl_get_feature_sse(0)), mean[2])), active_pixels);

	__m128 diff, scale;
	diff = _mm_sub_ps(ccl_get_feature_sse(1), mean[3]);
	scale = _mm_mul_ps(diff, diff);
	diff = _mm_sub_ps(ccl_get_feature_sse(2), mean[4]);
	scale = _mm_add_ps(scale, _mm_mul_ps(diff, diff));
	diff = _mm_sub_ps(ccl_get_feature_sse(3), mean[5]);
	scale = _mm_add_ps(scale, _mm_mul_ps(diff, diff));
	scales[3] = _mm_mask_ps(scale, active_pixels);

	scales[4] = _mm_mask_ps(_mm_fabs_ps(_mm_sub_ps(ccl_get_feature_sse(4), mean[6])), active_pixels);

	diff = _mm_sub_ps(ccl_get_feature_sse(5), mean[7]);
	scale = _mm_mul_ps(diff, diff);
	diff = _mm_sub_ps(ccl_get_feature_sse(6), mean[8]);
	scale = _mm_add_ps(scale, _mm_mul_ps(diff, diff));
	diff = _mm_sub_ps(ccl_get_feature_sse(7), mean[9]);
	scale = _mm_add_ps(scale, _mm_mul_ps(diff, diff));
	scales[5] = _mm_mask_ps(scale, active_pixels);
}

ccl_device_inline void filter_calculate_scale_sse(__m128 *scale)
{
	scale[0] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[0]), _mm_set1_ps(0.01f)));
	scale[1] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[1]), _mm_set1_ps(0.01f)));
	scale[2] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[2]), _mm_set1_ps(0.01f)));
	scale[6] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(scale[4]), _mm_set1_ps(0.01f)));

	scale[7] = scale[8] = scale[9] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[5])), _mm_set1_ps(0.01f)));
	scale[3] = scale[4] = scale[5] = _mm_rcp_ps(_mm_max_ps(_mm_hmax_ps(_mm_sqrt_ps(scale[3])), _mm_set1_ps(0.01f)));
}


CCL_NAMESPACE_END
